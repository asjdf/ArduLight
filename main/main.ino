// 2019-05-18 狗子 答应自己放下
#include "./Adafruit_NeoPixel.h"

//WS2812B Config
#define PIXEL_PIN   0    // 连接至灯带的数字IO 之前是14
#define PIXEL_COUNT 8    // led数
Adafruit_NeoPixel ws2812 = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

bool change = true;
uint8_t red, green, blue;
float brightness = 1.0;
long int flashTimer;

//创建wifi
#include <ESP8266WiFi.h>
IPAddress local_IP(192,168,1,1);  //申明各IP地址    软热点的地址
IPAddress gateway(192,168,1,1);  //网关地址
IPAddress subnet(255,255,255,0); //子网掩码

//交给setting.ino处理
char APssid[17]; //创建的ap的ssid
char APpassword[17]; //创建的ap的密码

/****************************web START**********************************/
#include <ESP8266WebServer.h>//创建web服务
#include <WiFiClient.h>
ESP8266WebServer webserver ( 80 );//设置网页服务监听端口
/****************************web END************************************/

/***********************通信解包用************************/
void SerialProcessing();
//定义一个comdata字符串变量，赋初值为空值
String comdata = "";
//numdata是分拆之后的数字数组，mark用于确认是否要进行数据处理，FinalData是最终接收的数据
int numdata[4] = {0};//数组长度为5
int mark = 0;
int FinalData[3];//数组长度为4
//j是分拆之后数字数组的位置记数
int j;
/*********************计算运行时间用**********************/
unsigned long starttime;
unsigned long stoptime;
unsigned long looptime;
/*************************end***************************/

/****************************setting START******************************/
#include <EEPROM.h>
#include "./EDB.h"
#include <FS.h>

#define defaultAPname "BigWhite"
#define defaultAPpassword "20020528"

#define TABLE1_SIZE 1024//表大小
#define TABLE2_SIZE 512
char* db1_name = "/db/edb_1.db";
char* db2_name = "/db/edb_2.db";
File db1File;
File db2File;
struct LogMainSetting {
    int id;
    char data[17];
}LogMainSetting;
struct LogExpendSetting {
    int id;
    int data;
}LogExpendSetting;
// 写/读方法
inline void writer1 (unsigned long address, const byte* data, unsigned int recsize) {
    db1File.seek(address, SeekSet);
    db1File.write(data,recsize);
    db1File.flush();
}
inline void reader1 (unsigned long address, byte* data, unsigned int recsize) {
    db1File.seek(address, SeekSet);
    db1File.read(data,recsize);
}
inline void writer2 (unsigned long address, const byte* data, unsigned int recsize) {
    db2File.seek(address, SeekSet);
    db2File.write(data,recsize);
    db2File.flush();
}
inline void reader2 (unsigned long address, byte* data, unsigned int recsize) {
    db2File.seek(address, SeekSet);
    db2File.read(data,recsize);
}

// 创建EDB项目 链接写/读方法
EDB db1(&writer1, &reader1);
EDB db2(&writer2, &reader2);

void configLoad();//加载设置
void table1Setup();//表1初始化
void table2Setup();
void table1save(int saveID,char* saveMessage);//表1储存
void table2save(int saveID,int saveMessage);
void select1All();//表1遍历并输出
void printError(EDB_Status err);//错误输出
/****************************setting END********************************/

void setup(){
  // 启动通讯
  Serial.begin(115200);
  delay(10);
  Serial.println("Booting");

  EEPROM.begin(4096);

  ws2812.begin();
  ws2812.show(); // Initialize all pixels to 'off'

  SPIFFS.begin();
  delay(500);//后期再调整@!
  configLoad();//加载设置
  WiFi.softAPConfig(local_IP, gateway, subnet);//设置ap的参数
  WiFi.softAP(APssid, APpassword);
  
  webserver.on("/inline", []() {
    webserver.send(200, "text/plain", "this works as well");
  });
  webserver.on("/",handleRoot);
  webserver.on("/rgb", HTTP_GET, handleGetRGB);
  webserver.on("/ExpendSetting", HTTP_GET, handleUploadSetting);
  webserver.onNotFound(handleWebRequests);//负责从spiffs加载一些没有定义的页面 以及处理404

  webserver.begin();//启动网页服务
  Serial.println ( "HTTP server started" );
  flashTimer = millis();
}

void loop(){
  if (change){
    leds();
    change = false;
  }
  SerialProcessing();
  webserver.handleClient();//web
  // Serial.println("loop");
}

void SerialProcessing(){//串口数据处理
  starttime = millis();//计时

  //j是分拆之后数字数组的位置记数
  j = 0;
  if(mark == 1){//如果接收到数据则执行comdata分析操作，否则什么都不做。
  /*******************对字符串进行拆包************************
   * 定义封包方式:"数据1,数据2,数据3,数据4"
   * 数据1=平移角度；数据2=转向角度；数据3=电机前进方向；数据4=灯光；数据5=校验（数据1+数据2+数据3+数据4）
  ********************************************************/
    //以串口读取字符串长度循环，
    for(int i = 0; i < comdata.length() ; ++i){
    //逐个分析comdata[i]字符串的文字，如果碰到文字是分隔符（这里选择逗号分割）则将结果数组位置下移一位
    //即比如11,22,33,55开始的11记到numdata[0];碰到逗号就j等于1了，
    //再转换就转换到numdata[1];再碰到逗号就记到numdata[2];以此类推，直到字符串结束
      if(comdata[i] == ',')
      {
        j++;
      }
      else
      {
        //如果没有逗号的话，就将读到的数字*10加上以前读入的数字，
        //并且(comdata[i] - '0')就是将字符'0'的ASCII码转换成数字0（下面不再叙述此问题，直接视作数字0）。
        //比如输入数字是12345，有5次没有碰到逗号的机会，就会执行5次此语句。
        //因为左边的数字先获取到，并且numdata[0]等于0，
        //所以第一次循环是numdata[0] = 0*10+1 = 1
        //第二次numdata[0]等于1，循环是numdata[0] = 1*10+2 = 12
        //第三次是numdata[0]等于12，循环是numdata[0] = 12*10+3 = 123
        //第四次是numdata[0]等于123，循环是numdata[0] = 123*10+4 = 1234
        //如此类推，字符串将被变成数字0。
        numdata[j] = numdata[j] * 10 + (comdata[i] - '0');
      }
    }
    Serial.print("Receive:");
    Serial.println(comdata);

    //校验数据并储存
    if(numdata[3] == numdata[0] + numdata[1] + numdata[2]){
      Serial.println("Calibration success!");
      red = numdata[0];
      green = numdata[1];
      blue = numdata[2];
      change = true;
    }else{
      Serial.println("Check error!");
    }

    //串口返回接收到的数据
    for(int i = 0; i < 4; ++i){
      Serial.print("numdata ");
      Serial.print(i);
      Serial.print(" = ");
      Serial.println(numdata[i]);
      numdata[i] = 0;
    }
    /*************************end***************************/
    stoptime = millis();//计时
    looptime = stoptime - starttime;//计时
    Serial.print("Time spent:");
    Serial.print(looptime);
    Serial.println("ms");

    //comdata的字符串已经全部转换到numdata了，清空comdata以便下一次使用，
    //如果不请空的话，本次结果极有可能干扰下一次。
    comdata = String("");
    //输出之后必须将读到数据的mark置0，不置0下次循环就不能使用了。
    mark = 0;
  }
}

/****************************灯光控制 START******************************/
void leds() {
  for (int i=0; i<ws2812.numPixels(); i++) {
    ws2812.setPixelColor(i, ws2812.Color(brightness*(float)red, brightness*(float)green, brightness*(float)blue));
  }
  ws2812.show();
  delay(20);
}
/****************************灯光控制 START******************************/

/****************************web START**********************************/
void handleGetRGB(){
  if (webserver.args()){
    if (webserver.hasArg("red"))
    {
      red = webserver.arg("red").toInt();
      Serial.print("Red:");
      Serial.println(red);
    }
    if (webserver.hasArg("green"))
    {
      green = webserver.arg("green").toInt();
      Serial.print("Green:");
      Serial.println(green);
    }
    if (webserver.hasArg("blue"))
    {
      blue = webserver.arg("blue").toInt();
      Serial.print("Blue:");
      Serial.println(blue);
    }
  }
  change = true;
  webserver.send ( 200, "text/plain", "ok");
}
void handleUploadSetting(){//接收设置
  String message = "Seceived";
  if (webserver.args())
  {
    if (webserver.hasArg("red"))
    {
      red = webserver.arg("red").toInt();
      table2save(1,red);
      Serial.print("Red:");
      Serial.println(red);
    }
    if (webserver.hasArg("green"))
    {
      green = webserver.arg("green").toInt();
      table2save(2,green);
      Serial.print("Green:");
      Serial.println(green);
    }
    if (webserver.hasArg("blue"))
    {
      blue = webserver.arg("blue").toInt();
      table2save(3,blue);
      Serial.print("Blue:");
      Serial.println(blue);
    }
    if (webserver.hasArg("APssid") || webserver.hasArg("APpassword")){
      if (webserver.hasArg("APssid")){
        if (webserver.arg("APssid").length() >= 8 && webserver.arg("APssid").length() <= 16)
        {
          strncpy(APssid, webserver.arg("APssid").c_str(), 16);//.c_str()将string型转换为char
          table1save(1, APssid);
        }else{
          message += "APssid wrong";
        }
      }
      if (webserver.hasArg("APpassword")){
        if (webserver.arg("APpassword").length() >= 8 && webserver.arg("APpassword").length() <= 16)
        {
          strncpy(APpassword, webserver.arg("APpassword").c_str(), 16);//.c_str()将string型转换为char
          table1save(2, APpassword);
        }else{
          message += "APpassword wrong";
        }
      }
      webserver.send ( 200, "text/html", message);
      WiFi.disconnect(true);
      WiFi.softAP(APssid, APpassword);//重新开启ap
    }
  }else{
    message += "no data";
  }
  webserver.send ( 200, "text/html", message);
}
void handleRoot(){
  webserver.sendHeader("Location", "/index.html",true);   //重定向
  webserver.send(302, "text/plane","");
}
String getContentType(String filename){  
  if(webserver.hasArg("download")) return "application/octet-stream";  
  else if(filename.endsWith(".htm")) return "text/html";  
  else if(filename.endsWith(".html")) return "text/html";  
  else if(filename.endsWith(".css")) return "text/css";  
  else if(filename.endsWith(".js")) return "application/javascript";  
  else if(filename.endsWith(".png")) return "image/png";  
  else if(filename.endsWith(".gif")) return "image/gif";  
  else if(filename.endsWith(".jpg")) return "image/jpeg";  
  else if(filename.endsWith(".ico")) return "image/x-icon";  
  else if(filename.endsWith(".xml")) return "text/xml";  
  else if(filename.endsWith(".pdf")) return "application/x-pdf";  
  else if(filename.endsWith(".zip")) return "application/x-zip";  
  else if(filename.endsWith(".gz")) return "application/x-gzip";  
  return "text/plain";  
}  
/* NotFound处理 
 * 用于处理没有注册的请求地址 
 * 一般是处理一些页面请求 
 */  
void handleWebRequests() {  
  String path = webserver.uri();  
  Serial.print("load url:");  
  Serial.println(path);  
  String contentType = getContentType(path);  
  String pathWithGz = path + ".gz";  
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  
    if(SPIFFS.exists(pathWithGz))  
      path += ".gz";  
    File file = SPIFFS.open(path, "r");  
    size_t sent = webserver.streamFile(file, contentType);  
    file.close();  
    return;  
  }  
  String message = "File Not Found\n\n";  
  message += "URI: ";  
  message += webserver.uri();  
  message += "\nMethod: ";  
  message += ( webserver.method() == HTTP_GET ) ? "GET" : "POST";  
  message += "\nArguments: ";  
  message += webserver.args();  
  message += "\n";  
  for ( uint8_t i = 0; i < webserver.args(); i++ ) {  
    message += " " + webserver.argName ( i ) + ": " + webserver.arg ( i ) + "\n";  
  }  
  webserver.send ( 404, "text/plain", message );  
}
/****************************web END************************************/

/****************************setting START******************************/
void configLoad() {
  //此加载函数自带初始化 所以去除后部configReset() configSave()
  if (SPIFFS.exists(db1_name)) {

      db1File = SPIFFS.open(db1_name, "r+");

      if (db1File) {
          Serial.print("Opening current table1... ");
          EDB_Status result = db1.open(0);
          if (result == EDB_OK) {
              Serial.println("DONE");
          } else {
              Serial.println("ERROR");
              Serial.println("Did not find database in the file " + String(db1_name));
              Serial.print("Creating new table1... ");
              db1.create(0, TABLE1_SIZE, (unsigned int)sizeof(LogMainSetting));
              table1Setup();//保存初始化数据
              Serial.println("DONE");
              return;
          }
      } else {
          Serial.println("Could not open file " + String(db1_name));
          return;
      }
  } else {
      Serial.print("Creating table1... ");
      // 创建一个以0位作为起始的表
      db1File = SPIFFS.open(db1_name, "w+");
      db1.create(0, TABLE1_SIZE, (unsigned int)sizeof(LogMainSetting));
      Serial.println("DONE");//保存初始化数据
      table1Setup();
  }
  //开始读取ap设置
  EDB_Status result1 = db1.readRec(1, EDB_REC LogMainSetting);
  if (result1 == EDB_OK)
  {
      //APssid = LogMainSetting.data;//采用此方法将导致地址不正确
      strncpy(APssid, LogMainSetting.data, 16);
      Serial.print("load APssid: ");
      Serial.println(APssid);
  }
  else printError(result1);
  EDB_Status result2 = db1.readRec(2, EDB_REC LogMainSetting);
  if (result2 == EDB_OK)
  {
      //APpassword = LogMainSetting.data;//采用此方法将导致地址不正确
      strncpy(APpassword, LogMainSetting.data, 16);
      Serial.print("load APpassword: ");
      Serial.println(APpassword);
  }
  else printError(result2);
  Serial.println(APssid);
  db1File.close();

  if (SPIFFS.exists(db2_name)) {

      db2File = SPIFFS.open(db2_name, "r+");

      if (db2File) {
          Serial.print("Opening current table2... ");
          EDB_Status result = db2.open(0);
          if (result == EDB_OK) {
              Serial.println("DONE");
          } else {
              Serial.println("ERROR");
              Serial.println("Did not find database in the file " + String(db2_name));
              Serial.print("Creating new table2... ");
              db2.create(0, TABLE2_SIZE, (unsigned int)sizeof(LogExpendSetting));
              table2Setup();//保存初始化数据
              Serial.println("DONE");
              return;
          }
      } else {
          Serial.println("Could not open file " + String(db2_name));
          return;
      }
  } else {
      Serial.print("Creating table2... ");
      // 创建一个以0位作为起始的表
      db2File = SPIFFS.open(db2_name, "w+");
      db2.create(0, TABLE2_SIZE, (unsigned int)sizeof(LogExpendSetting));
      Serial.println("DONE");//保存初始化数据
      table2Setup();
  }
  //开始读取灯灯设置
  db2File = SPIFFS.open(db2_name, "r+");
  db2.open(0);
  for (int i = 1; i <= db2.count(); ++i)
  {
    Serial.print(i);
    EDB_Status result = db2.readRec(i, EDB_REC LogExpendSetting);
    if (result == EDB_OK)
    {
      switch (i){
        case 1:
          red = LogExpendSetting.data;
          Serial.print("Red:");
          Serial.println(red);
          break;
        case 2:
          green = LogExpendSetting.data;
          Serial.print("Green:");
          Serial.println(green);
          break;
        case 3:
          blue = LogExpendSetting.data;
          Serial.print("Blue:");
          Serial.println(blue);
          break;
        default:
          break;
      }
    }
    else printError(result);
  }
  db2File.close();
}

void table1Setup() {
    Serial.print("table1 setup... ");
    LogMainSetting.id = 1;
    strncpy(LogMainSetting.data, defaultAPname, 16);
    EDB_Status result = db1.appendRec(EDB_REC LogMainSetting);
    if (result != EDB_OK) printError(result);

    LogMainSetting.id = 2;
    strncpy(LogMainSetting.data, defaultAPpassword, 16);
    EDB_Status result1 = db1.appendRec(EDB_REC LogMainSetting);
    if (result != EDB_OK) printError(result1);

    LogMainSetting.id = 3;
    strncpy(LogMainSetting.data, "0.1.0", 16);
    EDB_Status result2 = db1.appendRec(EDB_REC LogMainSetting);
    if (result != EDB_OK) printError(result2);

    Serial.println("DONE");
}

void table1save(int saveID,char* saveMessage) {
    db1File = SPIFFS.open(db1_name, "r+");
    db1.open(0);
    LogMainSetting.id = saveID;
    strncpy(LogMainSetting.data, saveMessage, 16);
    EDB_Status result = db1.updateRec(saveID, EDB_REC LogMainSetting);
    if (result != EDB_OK) printError(result);
    db1File.close();
}
void select1All(){
  db1File = SPIFFS.open(db1_name, "r+");
  db1.open(0);
  for (int recno = 1; recno <= db1.count(); recno++)
  {
    EDB_Status result = db1.readRec(recno, EDB_REC LogMainSetting);
    if (result == EDB_OK)
    {
      Serial.print("Recno: ");
      Serial.print(recno);
      Serial.print(" ID: ");
      Serial.print(LogMainSetting.id);
      Serial.print(" Temp: ");
      Serial.println(LogMainSetting.data);
    }
    else printError(result);
  }
  db1File.close();
}
void table2Setup()//初始化表2 num_recs为创建的项目数
{
    Serial.print("table2 setup... ");
    for (int recno = 1; recno <= 3; recno++)
    {
        LogExpendSetting.id = recno;
        LogExpendSetting.data = 255;
        EDB_Status result = db2.appendRec(EDB_REC LogExpendSetting);
        Serial.println(recno);
        if (result != EDB_OK) printError(result);
    }
    Serial.println("DONE");
}
void table2save(int saveID,int saveMessage){//将数据存入表2
    db2File = SPIFFS.open(db2_name, "r+");
    db2.open(0);
    Serial.println(saveMessage);
    LogExpendSetting.id = saveID;
    LogExpendSetting.data = saveMessage;
    EDB_Status result = db2.updateRec(saveID, EDB_REC LogExpendSetting);
    if (result != EDB_OK) printError(result);
    db2File.close();
}
void printError(EDB_Status err)
{
  Serial.print("ERROR: ");
  switch (err)
  {
    case EDB_OUT_OF_RANGE:
      Serial.println("Recno out of range");
      break;
    case EDB_TABLE_FULL:
      Serial.println("Table full");
      break;
    case EDB_OK:
    default:
      Serial.println("OK");
      break;
  }
}
/****************************setting END********************************/