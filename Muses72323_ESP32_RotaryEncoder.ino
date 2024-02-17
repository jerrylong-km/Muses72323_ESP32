#include <AiEsp32RotaryEncoder.h>
#include <Muses72323.h>
#include <EEPROM.h>
/*
   使用ESP32-S3-DevKitC-1模块控制Muses72323电子音量芯片
   By Jerry Long， 10 Feb 2024

   Muses72323芯片SPI接线：
      Latch(20#)——>SDL——>GPIO10(SS)
      Clock(19#)——>SCL——>GPIO12(SCK)
      Data(18#) ——>SDA——>GPIO11(MOSI)

   Muses72323芯片数字供电：
      D_IN(16#) ——>5V
      D_REF(32#)——>D_GND

   Muses72323芯片模拟供电：
      外接±15V电源，地A_GND
*/
/*
旋转编码器的硬件连接

Rotary encoder side    ESP32 side  
-------------------    ---------------------------------------------------------------------
CLK (A pin)            GPIO39
DT (B pin)             GPIO40
SW (button pin)        GPIO38
GND                    GND
VCC                    5V (设置 ROTARY_ENCODER_VCC_PIN -1) 

*/

/////////////////////////////////////////////
////           旋转编码器定义部分          ////
/////////////////////////////////////////////
#if defined(ESP8266)
#define ROTARY_ENCODER_A_PIN D6
#define ROTARY_ENCODER_B_PIN D5
#define ROTARY_ENCODER_BUTTON_PIN D7
#else
#define ROTARY_ENCODER_A_PIN 39
#define ROTARY_ENCODER_B_PIN 40
#define ROTARY_ENCODER_BUTTON_PIN 38
#endif
#define ROTARY_ENCODER_VCC_PIN -1 /* 当旋转编码器的供电VCC直接接板子输出5V时，该值设为-1 */

//根据所用的旋转编码器设置 - 尝试 1,2 or 4 以得到所需的效果
//#define ROTARY_ENCODER_STEPS 1
//#define ROTARY_ENCODER_STEPS 2
#define ROTARY_ENCODER_STEPS 4

//定义旋转编码器对象
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

/////////////////////////////////////////////
////        Muses72323芯片定义部分         ////
/////////////////////////////////////////////

//定义Muses72323芯片的地址（根据ADR0-#30和ADR1-#31脚位的高低电位）
static const byte MUSES_ADDRESS = 0;

static Muses72323 Muses(MUSES_ADDRESS); //根据上述芯片地址创建Muses72323对象


//定义默认音量，当前音量，最大和最小音量
static Muses72323::volume_t defaultVolume = -50; //重启后的默认音量-50dB
static Muses72323::volume_t currentVolume = -20; //当前音量，-20dB开始
int MaxVolume = 0; //最大音量0dB
int MinVolume = -479; //最小音量-111.75dB

/////////////////////////////////////////////
////            EEPROM定义部分            ////
/////////////////////////////////////////////
//定义使用的EEPROM容量大小为5(0，音量；1，音源；2，平衡/单端；3，静音；4，胆管使用时间)
#define EEPROM_SIZE 5
#define EEPROM_VOLUME_ADDRESS    0
#define EEPROM_INPUT_ADDRESS     1
#define EEPROM_BAL_SE_ADDRESS    2
#define EEPROM_MUTE_ADDRESS      3
#define EEPROM_TUBE_HRS_ADDRESS  4

void rotary_onButtonClick()
{
	static unsigned long lastTimePressed = 0;
	//ignore multiple press in that time milliseconds
	if (millis() - lastTimePressed < 500)
	{
		return;
	}
	lastTimePressed = millis();
	Serial.print("button pressed ");
	Serial.print(millis());
	Serial.println(" milliseconds after restart");
}

void rotary_loop()
{
	//dont print anything unless value changed
	if (rotaryEncoder.encoderChanged())
	{
		currentVolume = rotaryEncoder.readEncoder();
    Muses.setVolume(currentVolume, currentVolume);
    //音量值发生变化时，将当前音量值写入EEPROM
    EEPROM.write(EEPROM_VOLUME_ADDRESS, currentVolume);
    EEPROM.commit();
		Serial.print("currentVolume: ");
		Serial.println(currentVolume);
	}
	if (rotaryEncoder.isEncoderButtonClicked())
	{
		rotary_onButtonClick();
	}
}

void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}

void setup()
{
	Serial.begin(115200);

  //初始化EEPROM
  EEPROM.begin(EEPROM_SIZE);
  
	//初始化旋转编码器
	rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);
	//设置编码器是否循环
	bool circleValues = false;
	//设置编码器的取值范围（0, 479），对应的音量衰减范围（0, -111.75dB）
	rotaryEncoder.setBoundaries(0, 479, circleValues);

	//设置是否使用加速功能
	//rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
	rotaryEncoder.setAcceleration(80); //数值越大加速越快

  //初始化Muses72323
  Muses.begin();  
  
  //设置Muses72323状态
  Muses.setExternalClock(false); //不使用外部时钟，使用芯片内置的时钟
  Muses.setZeroCrossingOn(true); //启用ZeroCrossing功能

  //启动时设置为上次关机时音量，并同时将旋转编码器位置设定为相应数值
  currentVolume = EEPROM.read(EEPROM_VOLUME_ADDRESS);
  Muses.setVolume(currentVolume, currentVolume);
  rotaryEncoder.setEncoderValue(currentVolume);
}

void loop()
{
	//in loop call your custom function which will process rotary encoder values
	rotary_loop();
	delay(50); //or do whatever you need to do...
}
