# ArduLight
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fasjdf%2FArduLight.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fasjdf%2FArduLight?ref=badge_shield)

A simple network lamp consisting of esp8266 and ws2812

一个由ESP8266和WS2812组成的可无线控制的物联网灯

---

##### 目录结构:

web--控制页面代码(自己调试时使用的

main-程序文件

---

#### 元器件:

   ESP8266

   WS2812(我用的是8个灯珠的那种)

   5v电源

   balabala

---

#### 电路连接:

   接电源什么的就不说了

   我的代码中 ESP8266的0号io(第5行) 也就是nodemcu的D3口接WS2812灯带的信号线

   另外保证共地

---

#### 使用说明

如果不出意外 烧写完成并上电后

芯片将创建名为"BigWhite"的热点 密码为"20020528" (给EX的生日礼物,如有需要请自行修改51行和52行

连上热点 访问http://192.168.1.1

接下来的我相信你们都会用啦(运行截图什么的已经没有了 非常抱歉 因为灯已经不在我这了)

有问题提issue

---

分手快乐

## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fasjdf%2FArduLight.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fasjdf%2FArduLight?ref=badge_large)