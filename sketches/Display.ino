// СКЕТЧ ДЛЯ ПЛАТЫ ARDUINO UNO


// БИБЛИОТЕКИ для проекта
#include <Servo.h> 
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h> 
#include <EEPROM.h> 
#include <TimeLib.h>
// Конец 

#define BTN_PIN      2   // Пин кнопки открытия(записи)
#define DOOR_PIN     3   // Пин концевика двери
// пины RFID замка          
#define RST_PIN      4   
#define CS_PIN       5   
// конец                    
#define SERVO_PIN       6   // Пин серво
#define ENTER_PIN       7   // Пин датчика движения на входе в дом
#define BUZZER_PIN      8   // Пин пищалки
#define RED_LED_PIN     9   // Пин красного светодиода
#define GREEN_LED_PIN   10  // Пин зеленого светодиода
#define LIVING_PIN      12  // Пин датчика движения на входе
#define MIC_PIN         A0  // Пин датчика звука
#define SUN_PIN         A2  // Пин датчика освещенности

#define EE_START_ADDR   0   // Начальный адрес в EEPROM
#define EE_KEY        100   // Ключ EEPROM, для проверки на первое вкл.
#define LOCK_TIMEOUT  1000  // Время до блокировки замка после закрытия двери в мс 
#define MAX_TAGS        3   // Максимальное количество хранимых меток - ключей 
#define ASCII_CONVERT '0'   // Конвертация числа(для парсера)

#define DECLINE 0   // Отказ
#define SUCCESS 1   // Успешно
#define SAVED   2   // Новая метка записана
#define DELITED 3   // Метка удалена

LiquidCrystal_I2C lcd( 0x27 ,20,4);  // Объект экрана 
MFRC522 rfid(CS_PIN, RST_PIN);  // Обьект RFID
Servo doorServo;                // Объект серво

bool isOpen(void) {             // Функция должна возвращать true, если дверь физически открыта
  return digitalRead(DOOR_PIN); // Если дверь открыта - концевик размокнут, на пине HIGH
}
void lock(void) {               // Функция должна блокировать замок или нечто иное
  doorServo.attach(SERVO_PIN);
  doorServo.write(170);         // Для примера - запиранеие замка при помощи серво
  delay(1000);
  doorServo.detach();           // Детачим серво, чтобы не хрустела
  Serial.println("$80;");
}
void unlock(void) {             // Функция должна разблокировать замок или нечто иное
  doorServo.attach(SERVO_PIN);
  doorServo.write(10);          // Для примера - отпирание замка при помощи серво
  delay(1000);
  doorServo.detach();           // Детачим серво, чтобы не хрустела
  Serial.println("$81;");
}

bool times = false;       // Флаг для времени
bool locked = true;       // Флаг состояния замка
bool LED = false;         // флаг для света
bool needLock = false;    // Служебный флаг
uint8_t savedTags = 0;    // кол-во записанных меток
byte spuffer[10];          // буффер парсера
// служебные строки
String strtimes;          
String temp;              
String humi;


//Функция настройки 
void setup(){
 // инициализация
 Serial.begin(115200);
 SPI.begin();
 rfid.PCD_Init();
 lcd.init();  
 lcd.backlight(); 
 // Настройка пинов
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  
 uint32_t start = millis();        // Отслеживание длительного удержания кнопки после включения
  bool needClear = 0;               // Чистим флаг на стирание
  while (!digitalRead(BTN_PIN)) {   // Пока кнопка нажата
    if (millis() - start >= 3000) { // Встроенный таймаут на 3 секунды
      needClear = true;             // Ставим флаг стирания при достижении таймаута
      indicate(DELITED);            // Подаем сигнал удаления
      break;                        // Выходим из цикла
    }
  }
 // Инициализация EEPROM
  if (needClear or EEPROM.read(EE_START_ADDR) != EE_KEY) { // при первом включении или необходимости очистки ключей
    for (uint16_t i = 0; i < EEPROM.length(); i++) EEPROM.write(i, 0x00); // Чистим всю EEPROM
    EEPROM.write(EE_START_ADDR, EE_KEY);                   // Пишем байт-ключ
  } else {                                                 // Обычное включение
    savedTags = EEPROM.read(EE_START_ADDR + 1);            // Читаем кол-во меток в памяти
  }
  // Начальное состояние замка
  if (savedTags > 0) {      // Если метки в памяти есть
    if (isOpen()) {         // И дверь сейчас открыта
      ledSetup(SUCCESS);    // Зеленый лед
      locked = false;       // Замок открыт
      unlock();             // На всякий случай дернем замок
    } else {                // Метки есть, но дверь закрыта
      ledSetup(DECLINE);    // Красный лед
      locked = true;        // Замок закрыт
      lock();               // Блокируем замок
    }
  } else {                  // Если меток записано
    ledSetup(SUCCESS);      // Зеленый лед
    locked = false;         // Замок разлочен
    unlock();               // На всякий случай разблокируем замок
  }
  
 lcd.setCursor(1,0);
 lcd.print("Eclipse Home");
 lcd.setCursor(1,1);
 lcd.print("Powered by");
 lcd.setCursor(9,2);
 lcd.print("Bulochka");
 delay(2000);
 lcd.clear();
 lcd.print("Loading");
}

void loop(){
  if(parsing()){
  switch (spuffer[0]){
  case 0: //синхронизация времени(единичный запуск)
  for(int i = 1; i < sizeof(spuffer) + 1; i++){  
   strtimes += String(spuffer[i]);}
   tone(BUZZER_PIN,165,220);
   setTime(syncing(strtimes));
   tehy();
   times = true;
   Serial.println("$0;");
  break;
  case 1: //синхронизация датчиков 
    temp = String(spuffer[1]) + String(spuffer[2]);
    humi = String(spuffer[3]) + String(spuffer[4]);
    tone(BUZZER_PIN,165,220);
    lcd.print(".");
    Serial.println("$1;");
  break;
  case 2:
    indicate(SUCCESS);      // Зеленый лед + звук
    locked = false;         // Замок разлочен
    unlock();               // разблокируем замок
    break;
  case 3:
  break;
  case 4:
    signalization(true);
    break;
  case 5:
    Serial.println("none");
    break;
  case 6:
    temp = String(spuffer[1]) + String(spuffer[2]);
    humi = String(spuffer[3]) + String(spuffer[4]);
  break;
  case 7:
     Serial.println("none");
  break;
  }
  }
  
  static uint32_t lockTimeout;             // Таймер таймаута для блокировки замка
  
  // Открытие по нажатию кнопки изнутри
  if (locked and !digitalRead(BTN_PIN)) {  // Если дверь закрыта и нажали кнопку
    indicate(SUCCESS);                     // Зеленый лед
    unlock();                              // Разблокируем замок
    lockTimeout = millis();                // Запомнили время
    locked = false;                        // Замок разлочен
  }

  // Проверка концевика двери
  if (isOpen()) {                          // Если дверь открыта
    lockTimeout = millis();                // Обновляем таймер
  }

  // Блокировка замка по таймауту (ключей > 0, замок разлочен, таймаут вышел)
  if (savedTags > 0 and !locked and millis() - lockTimeout >= LOCK_TIMEOUT) {
    ledSetup(DECLINE); // Красный лед
    lock();            // Блокируем
    locked = true;     // Ставим флаг
  }

  // Поднесение метки
  static uint32_t rfidTimeout; // Таймаут рфид
  if (rfid.PICC_IsNewCardPresent() and rfid.PICC_ReadCardSerial()) { // Если поднесена карта
    if (isOpen() and !digitalRead(BTN_PIN) and millis() - rfidTimeout >= 500) { // И дверь открыта + кнопка нажата
      saveOrDeleteTag(rfid.uid.uidByte, rfid.uid.size);              // Сохраняем или удаляем метку
    } else if (locked) {                                             // Иначе если замок заблокирован
      if (foundTag(rfid.uid.uidByte, rfid.uid.size) >= 0) {          // Ищем метку в базе
        indicate(SUCCESS);                                           // Если нашли - подаем сигнал успеха
        unlock();                                                    // Разблокируем
        lockTimeout = millis();                                      // Обновляем таймаут
        locked = false;                                              // Замок разблокирован
      } else if (millis() - rfidTimeout >= 500) {                    // Метка не найдена (с таймаутом)
        indicate(DECLINE);                                           // Выдаем отказ
      }
    }
    rfidTimeout = millis();                                          // Обвновляем таймаут
  }
  // Перезагружаем RFID каждые 0.5 сек (для надежности)
  static uint32_t rfidRebootTimer = millis(); // Таймер
  if (millis() - rfidRebootTimer > 500) {     // Каждые 500 мс
    rfidRebootTimer = millis();               // Обновляем таймер
    digitalWrite(RST_PIN, HIGH);              // Дергаем резет
    delay(1);
    digitalWrite(RST_PIN, LOW);
    rfid.PCD_Init();                          // Инициализируем модуль
  }
  // Обновление времени на экране
  static uint32_t rebooterTime = millis(); // Таймер 
  if (millis() - rebooterTime > 1000 && times == true){     // Каждую секунду
    rebooterTime = millis();               // Обновляем таймер
    lcd.setCursor(6,3);                // Ставим курсор
    lcd.print(leadNull(hour())); lcd.print(":"); lcd.print(leadNull(minute()));  // Обновляем время на экране
    }
  static uint32_t rebooterSensors = millis();
  if (millis() - rebooterSensors > 60000 && times == true){ //обновление температуры и влажности в доме(раз в минуту),а так же передача полученного значения с фоторезистора
    int sun = 0;
    rebooterSensors = millis();
    lcd.setCursor(6,1);
    lcd.print(temp);lcd.print(char(223)); lcd.print("C");
    lcd.setCursor(6,2);
    lcd.print(humi);lcd.print("%");
    sun = map(analogRead(SUN_PIN), 0,512, 0, 3);
    Serial.print("$4");Serial.print(sun); Serial.println(";");
    }
  
}
// Устанавливаем состояние светодиодов
void ledSetup(bool state) {
  if (state){
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    }
}
// Звуковой сигнал + лед
void indicate(uint8_t signal) {
  ledSetup(signal); // Лед
  switch (signal) { // Выбираем сигнал
    case DECLINE:
      Serial.println("DECLINE");
      for (uint8_t i = 0; i < 2; i++) {
        tone(BUZZER_PIN, 100);
        delay(300);
        noTone(BUZZER_PIN);
        delay(100);
      }
      return;
    case SUCCESS:
      Serial.println("SUCCESS");
      tone(BUZZER_PIN, 890);
      delay(330);
      noTone(BUZZER_PIN);
      return;
    case SAVED:
      Serial.println("SAVED");
      for (uint8_t i = 0; i < 2; i++) {
        tone(BUZZER_PIN, 890);
        delay(330);
        noTone(BUZZER_PIN);
        delay(100);
      }
      return;
    case DELITED:
      Serial.println("DELITED");
      for (uint8_t i = 0; i < 3; i++) {
        tone(BUZZER_PIN, 890);
        delay(330);
        noTone(BUZZER_PIN);
        delay(100);
      }
      return;
  }
}
// Сравнение двух массивов известного размера
bool compareUIDs(uint8_t *in1, uint8_t *in2, uint8_t size) {
  for (uint8_t i = 0; i < size; i++) {  // Проходим по всем элементам
    if (in1[i] != in2[i]) return false; // Если хоть один не сошелся - массивы не совпадают
  }
  return true;                          // Все сошлись - массивы идентичны
}
// Поиск метки в EEPROM
int16_t foundTag(uint8_t *tag, uint8_t size) {
  uint8_t buf[8];   // Буфер метки
  uint16_t address; // Адрес
  for (uint8_t i = 0; i < savedTags; i++) { // проходим по всем меткам 
    address = (i * 8) + EE_START_ADDR + 2;  // Считаем адрес текущей метки
    EEPROM.get(address, buf);               // Читаем метку из памяти
    if (compareUIDs(tag, buf, size)) return address; // Сравниваем - если нашли возвращаем асдрес
  }
  return -1;                                // Если не нашли - вернем минус 1
}
// Удаление или запись новой метки
void saveOrDeleteTag(uint8_t *tag, uint8_t size) {
  int16_t tagAddr = foundTag(tag, size);                      // Ищем метку в базе
  uint16_t newTagAddr = (savedTags * 8) + EE_START_ADDR + 2;  // Адрес крайней метки в EEPROM
  if (tagAddr >= 0) {                                         // Если метка найдена - стираем
    for (uint8_t i = 0; i < 8; i++)  {                        // 8 байт
      EEPROM.write(tagAddr + i, 0x00);                        // Стираем байт старой метки
      EEPROM.write(tagAddr + i, EEPROM.read((newTagAddr - 8) + i)); // На ее место пишем байт последней метки
      EEPROM.write((newTagAddr - 8) + i, 0x00);               // Удаляем байт последней метки
    }
    EEPROM.write(EE_START_ADDR + 1, --savedTags);             // Уменьшаем кол-во меток и пишем в EEPROM
    indicate(DELITED);                                        // Подаем сигнал
  } else if (savedTags < MAX_TAGS) {                          // метка не найдена - нужно записать, и лимит не достигнут
    for (uint16_t i = 0; i < size; i++) EEPROM.write(i + newTagAddr, tag[i]); // Зная адрес пишем новую метку
    EEPROM.write(EE_START_ADDR + 1, ++savedTags);             // Увеличиваем кол-во меток и пишем
    indicate(SAVED);                                          // Подаем сигнал
  } else {                                                    // лимит меток при попытке записи новой
    indicate(DECLINE);                                        // Выдаем отказ
    ledSetup(SUCCESS);
  }
}

//Перевод строки в uint32_t
unsigned long syncing(String Str){
  unsigned long ULong = 0;
  for (int i = 0; i < Str.length(); i++) {                    // В цикле посимвольно переводим строку в unsigned long
     char c = Str.charAt(i);
     if (c < '0' || c > '9') break;
     ULong *= 10;
     ULong += (c - '0');
  }
  return ULong;
  }
//Функция добавляет ведущий ноль
String leadNull(int digits){                                     
  String out = "";
  if(digits < 10)
    out += "0";                                               
  return out + String(digits);
}
//Функция парсер
int parsing() {
  static bool parseStart = false;
  static byte counter = 0;
  if (Serial.available()) {
    char in = Serial.read();    
    if (in == '\n' || in == '\r') return 0; // игнорируем перевод строки    
    if (in == ';') {        // завершение пакета
      parseStart = false;
      return counter;
    }    
    if (in == '$') {        // начало пакета
      parseStart = true;
      counter = 0;
      return 0;
    }    
    if (parseStart) {       // чтение пакета
      // - '0' это перевод в число (если отправитель print)
      spuffer[counter] = in - ASCII_CONVERT;
      counter++;
    }
  }
  return 0;}
//Вывод полученных данных на экран 
void tehy(){
  lcd.clear();
  lcd.print("Eclipse Home");
  lcd.setCursor(0,1);
  lcd.print("temp: "); lcd.print(temp);lcd.print(char(223));lcd.print("C");
  lcd.setCursor(0,2);
  lcd.print("humi: ");lcd.print(humi);lcd.print("%");
  lcd.setCursor(0,3);
  lcd.print("time: "); lcd.print(leadNull(hour())); lcd.print(":"); lcd.print(leadNull(minute()));
  }
void signalization(bool some){
  if (some){
    some = false;
    tone(BUZZER_PIN, 2300);
    delay(2000);
    noTone(BUZZER_PIN);
    }
  }
// buf.crc = crc8_bytes((byte*)&buf, sizeof(buf) - 1); */
