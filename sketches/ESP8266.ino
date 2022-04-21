// СКЕТЧ ДЛЯ ПЛАТЫ nodeMCUv3 ESP8266
// Библиотеки проекта
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>                   
#include <ArduinoJson.h> 
#include <TimeLib.h>
#include <FS.h>
#include <ESP8266FtpServer.h>
#include <SimpleDHT.h>
#define converter '0'
// пины подключения
const byte pinDHT11 = 13;
const byte pinHeat = 2;
const byte relay = 5;
const byte pinEnter = 12;
const byte pinRoom = 14;
const byte relay2 = 4;

//
byte buffer[12];
bool locked = false;
bool ph_state = false;
bool enter_state = false;
bool room_state = false;
String ph;


SimpleDHT11 dht11;
FtpServer FTPserv;
MDNSResponder mdns;

//FTP
const char* login = "admin";
const char* fpassword = "9lCmvQu30NVW$E%3";

// Wi-Fi
const char* ssid = "Lerik";
const char* password = "11091981bul";

//некоторые переменные для работы с yandex погодой
String regionID = "213"; 
String SunriseTime, SunsetTime, Temperature;
char icon[20];

//некоторые статические переменные для работы скетча
static byte t = 0;
static byte h = 0;
static int photo = 0;

//инициализация
WiFiClient client;
ESP8266WebServer server(80);

void setup() {
  pinMode(relay, OUTPUT);
  pinMode(pinHeat, OUTPUT);
  pinMode(relay2, OUTPUT);
  delay(100);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.print("Connecting");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
   Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Syncing");
  while (!TimeAndWeather())
    delay(500);                          
  Serial.print("\ndone \ntime:");
  digitalClockDisplay();
  WeatherDisplay();
  if (mdns.begin("Eclipse", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }         
  SPIFFS.begin();
  server.begin();
  Serial.println("HTTP server started");
  FTPserv.begin(login,fpassword);
  Serial.println("FTP server started");
  Serial.println("done");
  Serial.println("connecting to arduino");
  while (!eclipse())
    delay(500);
  Serial.println("Connecting done");
  Serial.println("the system has ready for use");
  
  server.on("/door_status", [](){
    server.send(200, "text/plain", door_status());
    });  
  server.on("/door_open", [](){
    server.send(200, "text/plain", door_open());
    });
  server.on("/heating", [](){
    server.send(200,"text/plain", heating());
    }); 
  server.on("/heating_status", [](){
    server.send(200,"text/plain", heat_status());
    });
  server.on("/temp", [](){
    server.send(200,"text/plain", temp());
    });
  server.on("/humi", [](){
    server.send(200,"text/plain", humi());
    });
  server.on("/light", [](){
    server.send(200,"text/plain", light());
    });
  server.on("/light_status", [](){
    server.send(200,"text/plain", light_status());
    });
  server.onNotFound([](){                                                 // Описываем действия при событии "Не найдено"
  if(!handleFileRead(server.uri()))                                       // Если функция handleFileRead (описана ниже) возвращает значение false в ответ на поиск файла в файловой системе
      server.send(404, "text/plain", "Not Found");                        // возвращаем на запрос текстовое сообщение "File isn't found" с кодом 404 (не найдено)
  }); 
}

void loop() {
  server.handleClient();
  FTPserv.handleFTP();
  if (parsing()){
    switch(buffer[0]){
    case 8:
    if (buffer[1] == 0){ 
      Serial.println("locked");
      locked = true;
    }
    else {
      Serial.println("open");
      locked = false;
      }
    break;
    case 4:
    ph_state = true;
    delay(0);
    switch (buffer[1]){
      case 0:
      photo = 255;
      break;
      case 1:
      photo = 170;
      break;
      case 2:
      photo = 100;
      break;
      case 3:
      photo = 0;
      break;
      }
    break;
    }
  }
  static uint32_t timerS = millis();
  if (locked && movement() && timerS - millis() > 3000){
    Serial.println("$4;");
    delay(1000);
    timerS = millis();
    }
  static uint32_t timerE = millis();
  if (!locked && digitalRead(pinEnter) && timerE - millis() > 3000){
    analogWrite(relay2 ,photo);
    delay(1000);
    enter_state = true;
    timerE = millis();
  }
  static uint32_t timerR = millis();
  if (!locked && digitalRead(pinRoom) && timerR - millis() > 3000){
    analogWrite(relay, photo);
    delay(1000);
    room_state = true;
    timerR = millis();
    }
  static uint32_t rebooter = millis();
  if (room_state and rebooter - millis() > 5000){
    room_state = false;
    rebooter = millis();
    digitalWrite(relay, LOW);
    }
  if (enter_state and rebooter - millis() > 5000){
    enter_state = false;
    rebooter = millis();
    digitalWrite(relay2, LOW);
    }
}

String light(){
  byte state;
  if (digitalRead(relay))                                               // Если на пине реле высокий уровень   
    state = 0;                                                          //  то запоминаем, что его надо поменять на низкий
  else                                                                  // иначе
    state = 1;                                                          //  запоминаем, что надо поменять на высокий
  if (ph_state && state) analogWrite(relay, photo);
  else digitalWrite(relay, state);                                         // меняем значение на пине подключения реле
  return String(state); }
  
String light_status(){
  byte state;
  if (digitalRead(relay))
    state = 1;
  else 
    state = 0;
  return String(state);}
  
String door_open(){
  byte door_state;
  Serial.println("$2200;");
  door_state = 1;
  return String(door_state);
  }
  
String door_status(){
  byte door_state;
  if (locked) door_state = 0;
  else door_state = 1;
  return String(door_state);
}

String heating(){
  byte state;
  if (digitalRead(pinHeat))
    state = 0;
  else 
    state = 1;
  digitalWrite(pinHeat, state);
  return String(state);}
  
String heat_status(){
  byte state;
  if (digitalRead(pinHeat))
    state = 1;
  else 
    state = 0;
  return String(state);
  }
  
String temp(){
  return String(t);
  }
  
String humi(){
  return String(h);
  }

bool handleFileRead(String path){                                       // Функция работы с файловой системой
  if(path.endsWith("/")) path += "index.html";                          // Если устройство вызывается по корневому адресу, то должен вызываться файл index.html (добавляем его в конец адреса)
  String contentType = getContentType(path);                            // С помощью функции getContentType (описана ниже) определяем по типу файла (в адресе обращения) какой заголовок необходимо возвращать по его вызову
  if(SPIFFS.exists(path)){                                              // Если в файловой системе существует файл по адресу обращения
    File file = SPIFFS.open(path, "r");                                 //  Открываем файл для чтения
    size_t sent = server.streamFile(file, contentType);                 //  Выводим содержимое файла по HTTP, указывая заголовок типа содержимого contentType
    file.close();                                                       //  Закрываем файл
    return true;                                                        //  Завершаем выполнение функции, возвращая результатом ее исполнения true (истина)
  }
  return false;                                                         // Завершаем выполнение функции, возвращая результатом ее исполнения false (если не обработалось предыдущее условие)
}

String getContentType(String filename){                                 // Функция, возвращающая необходимый заголовок типа содержимого в зависимости от расширения файла
  if (filename.endsWith(".html")) return "text/html";                   // Если файл заканчивается на ".html", то возвращаем заголовок "text/html" и завершаем выполнение функции
  else if (filename.endsWith(".css")) return "text/css";                // Если файл заканчивается на ".css", то возвращаем заголовок "text/css" и завершаем выполнение функции
  else if (filename.endsWith(".js")) return "application/javascript";   // Если файл заканчивается на ".js", то возвращаем заголовок "application/javascript" и завершаем выполнение функции
  else if (filename.endsWith(".png")) return "image/png";               // Если файл заканчивается на ".png", то возвращаем заголовок "image/png" и завершаем выполнение функции
  else if (filename.endsWith(".jpg")) return "image/jpeg";              // Если файл заканчивается на ".jpg", то возвращаем заголовок "image/jpg" и завершаем выполнение функции
  else if (filename.endsWith(".gif")) return "image/gif";               // Если файл заканчивается на ".gif", то возвращаем заголовок "image/gif" и завершаем выполнение функции
  else if (filename.endsWith(".ico")) return "image/x-icon";            // Если файл заканчивается на ".ico", то возвращаем заголовок "image/x-icon" и завершаем выполнение функции
  return "text/plain";                                                  // Если ни один из типов файла не совпал, то считаем что содержимое файла текстовое, отдаем соответствующий заголовок и завершаем выполнение функции
}

bool eclipse(){                   
  bool value;
  dht11.read(pinDHT11, &t, &h, NULL);
  Serial.print("$1"); Serial.print(t); Serial.print(h); Serial.println(";");
  delay(3000);
  if (parsing()){
    switch (buffer[0]){
    case 1:
    Serial.println("done");
    Serial.print("$0"); Serial.print(now()); Serial.println(";");
    case 0:
    Serial.println("connecting on");
    value = true;
    }
  }else value = false;
  return value;
  }

bool movement(){
  if (digitalRead(pinEnter) || digitalRead(pinRoom)) return true;
  else return false;
  }
// функия работы погоды и времени
bool TimeAndWeather () {                                                    // Функция синхронизации времени работы программы с реальным временем и получения информации о погоде
  if (client.connect("yandex.com",443)) {                                   // Если удаётся установить соединение с указанным хостом (Порт 443 для https)
    client.println("GET /time/sync.json?geo=" + regionID + " HTTP/1.1\r\nHost: yandex.com\r\nConnection: keep-alive\r\n\r\n"); // Отправляем параметры запроса
    delay(200);                                                             // Даём серверу время, чтобы обработать запрос
    char endOfHeaders[] = "\r\n\r\n";                                       // Системные заголовки ответа сервера отделяются от остального содержимого двойным переводом строки
    if (!client.find(endOfHeaders)) {                                       // Отбрасываем системные заголовки ответа сервера
      Serial.println("Invalid response");                                   // Если ответ сервера не содержит системных заголовков, значит что-то пошло не так
      return false;                                                         // и пора прекращать всё это дело
    }
    const size_t capacity = 750;                                            // Эта константа определяет размер буфера под содержимое JSON (https://arduinojson.org/v5/assistant/)
    DynamicJsonBuffer jsonBuffer(capacity);                                 // Инициализируем буфер под JSON
    
    JsonObject& root = jsonBuffer.parseObject(client);                      // Парсим JSON-модержимое ответа сервера
    client.stop();                                                          // Разрываем соединение с сервером

    String StringCurrentTime = root["time"].as<String>().substring(0,10);   // Достаём значение реального текущего времени из JSON и отбрасываем от него миллисекунды
    String StringOffset =  root["clocks"][regionID]["offset"].as<String>(); // Достаём значение смещения времени по часовому поясу (в миллисекундах)
    SunriseTime =  root["clocks"][regionID]["sunrise"].as<String>();        // Достаём время восхода - Третий уровень вложенности пары ключ/значение clocks -> значение RegionID -> sunrise 
    SunsetTime =  root["clocks"][regionID]["sunset"].as<String>();          // Достаём время заката - Третий уровень вложенности пары ключ/значение clocks -> значение RegionID -> sunset
    Temperature =  root["clocks"][regionID]["weather"]["temp"].as<String>();// Достаём время заката - Четвёртый уровень вложенности пары ключ/значение clocks -> значение RegionID -> weather -> temp
    strcpy(icon, root["clocks"][regionID]["weather"]["icon"].as<String>().c_str());       // Достаём иконку - Четвёртый уровень вложенности пары ключ/значение clocks -> значение RegionID -> weather -> icon

    jsonBuffer.clear();                                                     // Очищаем буфер парсера JSON
    
    unsigned long CurrentTime = StringToULong(StringCurrentTime);           // Переводим значение реального времени в секундах, считанное с Яндекса, из String в unsigned long
    unsigned long Offset = StringToULong(StringOffset) / 1000;              // Переводим значение смещения времени по часовому поясу, считанное с Яндекса, из String в unsigned long и переводим его в секунды
    setTime(CurrentTime + Offset);                                          // Синхронизируем время
    
    return true;
  }
}

unsigned long StringToULong(String Str) {                     // Эта функция преобразует String в unsigned long
  unsigned long ULong = 0;
  for (int i = 0; i < Str.length(); i++) {                    // В цикле посимвольно переводим строку в unsigned long
     char c = Str.charAt(i);
     if (c < '0' || c > '9') break;
     ULong *= 10;
     ULong += (c - '0');
  }
  return ULong;
}

void digitalClockDisplay(){                                   // Эта функция выводит дату и время на монитор серийного порта
  Serial.print(leadNull(day()));
  Serial.print(".");
  Serial.print(leadNull(month()));
  Serial.print(".");
  Serial.print(year());
  Serial.print(" ");
  Serial.print(leadNull(hour()));
  Serial.print(":");
  Serial.print(leadNull(minute()));
  Serial.print(":");
  Serial.print(leadNull(second()));
  Serial.println();
}

String leadNull(int digits){                                    // Функция добавляет ведущий ноль
  String out = "";
  if(digits < 10)
    out += "0";                                               
  return out + String(digits);
}

void WeatherDisplay(){  
  char * out = strtok(icon,"-");        // Выделяем первую часть из строки до символа '-'
  while (out != NULL) {                 // Выделяем последующие части строки в цикле, пока значение out не станет нулевым (пустым)
      if (String(out) == "skc")         // Перебираем в условиях все возможные варианты, зашифрованные в названии иконки
        Serial.println("Yasno");
      else if (String(out) == "ovc")
        Serial.println("Pasmurno");
      else if (String(out) == "bkn")
        Serial.println("Oblachno");
      else if (String(out) == "ra")
        Serial.println("Dozhd'");
      else if (String(out) == "ts")
        Serial.println("Groza");
      else if (String(out) == "sn")
        Serial.println("Sneg");
      else if (String(out) == "bl")
        Serial.println("Metel'");
      else if (String(out) == "fg")
        Serial.println("Tuman");
      else if (String(out) == "n")
        Serial.println("\nTemnoe vremya sutok");
      else if (String(out) == "d")
        Serial.println("\nSvetloe vremya sutok");
      
      out = strtok(NULL,"-");              // Выделяем очередную часть
   }
}


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
      buffer[counter] = in - converter;
      counter++;
    }
  }
  return 0;} 
