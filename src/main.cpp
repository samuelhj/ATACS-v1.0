/*
  Höfundur: Samúel Þór Hjaltalín Guðjónsson
  samuel@ulfr.net
  * Útgáfa 2021-02-01
  * 
  Stutt lýsing:
    Forritið stýrir 6rása mosfet útgangsrás, snertiskjá og les einnig frá einum
    hitanema og einum þrýstinema. Gildin birtir það svo á skjánum.
    Frá skjánum tekur það valin gildi á hvaða þrýstingur á að vera í dekkjum og
    stillir þrýstingin í dekkjunum eftir því. Einnig fylgist það af og til með
    að réttur þrýstingur sé í dekkjum og ef það er ekki til staðar leiðréttir forritið
    þrýstingin í því dekki eða dekkjum sem um ræðir.

    Þetta forrit miðast við 2+4 kistu og með skynjarann á prentplötu
    Líklega er ekki mikið mál að breyta forritinu svo það lesi aðrar
    tegundir af skynjurum. Ef ekki er ætlunin að nota loftút eða auka úrtak frá
    loftkerfi má sleppa AIR_IN loka og tengja þann útgang frá MOSFET stýringu beint
    á segulrofa fyrir loftdælu og spara þannig bæði einn loftloka og aflestunarloka
    fyrir loftdælu.


    Inngangar eru eftirfarandi:
    * Þrýstinemi sem les þrýsting frá kistu.
    * Innra hitastig sem sýnir hita í bíl og á rás. (Verður fjarlægt í V2.0)

    Útgangar eru eftirfarandi:
    * 6 til að stýra loftlokum með MOSFET stýringu (Gefur jörð)
    * RS232 útgangur fyrir t.d. RS232 skjá eða samskipti við hvað annað sem mönnum
      gæti dottið í hug að bæta við seinna meir.
    * 15 pinna tengi fyrir SPI skjá.

      TODO OG ÖNNUR VANDAMÁL
      * x PWM fyrir backlight virkar ekki
      * x Klára tímasetningar fyrir loft í og úr dekkjunum
      * x Viðvörunarhluti
      * x úrhleypingar hluti
      * x Inndælingarhluti
      * x Baklýsing í menu
      * x Truflanir af völdum PWM vegna baklýsingar
      * x Virkja inndælingu/úrhleypingu dekkja úr menui
      * x EEPROM lestur og skrif fyrir stillingar.
      * x Viðvörunarhluti virkar ekki lengur! >:(
      * x Bæta tíma við inndælingu/úrhleypingu
      * þrepaskiptingu milli inndælingu/úrhleypingu m.v. þrýsting
      * adjust lúppa sem lærir á tímann sem tekur að dæla í dekk og hleypa úr
      * Valmöguleika til að stilla hvert dekk sér.
      * x Mæling dekkja í stillingu virkar ekki

      Nýtt 2020
      * Auka lykku sem dælir minna í einu
      * lengja tíma á standard dælingu/úrhleypingu
      * stoppa fyrr í þrýstingi í forvali
      * Sýna alltaf þrýsting kistu
      * minni nákvæmni á hærri þrýsting
      * Setja dekk sem lendir reglulega í "warningcheck" á sérstaka gjörgæslu (mæla oftar)

      * Libraries í notkun
      * TouchScreen.h: 


*/

#include <Arduino.h> //Við köllum á grunn library fyrir Arduino hlutann
#include <SPI.h> // Við þurfum SPI library fyrir samskipti við snertiskjá.
#include <EEPROM.h> // Við þurfum library til að skrifa og lesa EEPROM.

#include "TouchScreen.h" // Við þurfum library til að lesa snertingu af skjá.
#include <Adafruit_GFX.h> // Við þurfum library til að teikna á skjá.
#include <Adafruit_ILI9341.h> // Við þurfum library til að tala við ILI9341 stýringu á skjá.

// fastar sem eru bundnir við þetta tiltekna tæki.
//#define SERIALNUMBER "003" // Þetta ætti að vera lesið úr EEPROM...
#define VERSION "v1.2"
#define BUILDDATE "2020-03-08"
#define CALIBRATE ON // Ef calibrate er ON þá keyrir bara calibrate lúppan.

// Hér skilgreinum við fasta sem breytast ekki.
// Fastar tafir.
#define AIR_DELAY 1000 // Hve lengi við hinkrum meðan verið er að dæla í eða úr kistu. 1s er nægur tími.
#define TIMERTIRE 10
#define TIMER10PSI 45000 // 30s fyrir langa
#define TIMER5PSI 30000
#define TIMER1PSI 15000 // 7 sekúndur
#define TIMER025PSI 7000

// EEPROM minnishólf
#define ESERIALNUMBER 0 // Skilgreinum hvar seríal númer er geymt.
#define EBACKLIGHT 10 // Skilgreinum minnishólf fyrir baklýsingu
#define EPRESSURE 15 // Skilgreinum minnishólf fyrir þrýsting
#define EPRESSURE_LRT 20 // Skilgreinum minnishólf fyrir dekkjaþrýsting
#define EPRESSURE_LFT 25
#define EPRESSURE_RFT 30
#define EPRESSURE_RRT 35

// Val milli active high eða active low. Auðveldar portun á kóða fyrir aðrar útgangsstýringar.
#define OFF LOW
#define ON HIGH

// Skilgreinum pinna fyrir hvern segulloka.
// Þessi pinnaröð er miðað við Arduino digital pinnaröð samkvæmt ATmega1284, en miðast
// ekki við fýsíska pinna örtölvunnar. Þeir koma í sviga fyrir aftan athugasemd.
// Númer víra að lokum er svo aftast.
#define AIR_OUT 21   // Loft út af kistu (27) (6)
#define TIRE_LR 20   // vinstra afturdekk (26) (5)
#define TIRE_LF 19   // vinstra framdekk (25) (4)
#define TIRE_RF 18   // hægra  framdekk (24) (3)
#define TIRE_RR 17   // hægra afturdekk (23) (2)
#define AIR_IN 16  // Loft inn á kistu. (22) (1)


// Skilgreinum pinna fyrir skynjara.
#define P_SENSOR A0 // Þrýstingsnemi MPX5700 (40)
#define TEMP_POSITIVE A1 // Innri hitanemi pósitíft (39)
#define TEMP_NEGATIVE A2 // Innri hitanemi negatíft (38)
#define IGNITION 0 // Skynjun á hvort bíll sé í gangi eða svissað á. (1)

// Skilgreinum minnsta og mesta gildi sem snertingin má vera.
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

// Skilgreinum mesta og minnsta þrýsting á snertiskjáinn
#define MINPRESSURE 10
#define MAXPRESSURE 1000

// X+ X- Y+ Y- pinnar fyrir snertiskjá og mæla X/Y ás
#define YP A6  // Y+ (35)
#define XM A7  // X- (34)
#define YM 15  // Y- (21)
#define XP 12  // X+ (18)

// Við þurfum að þekkja viðnámið yfir snertiskjáinn
// milli  X+ and X-
// Má nota hefðbundin DMM til að mæla.
// Þessi mælist 291 Ohm
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 291); // Skilgreinum breytur fyrir snertiskjá.

#define TFT_CS 14 // CS er á pinna 14 (20)
#define TFT_DC 13 // D/C er á pinna 13 (19)
#define BACKLIGHT 4 // Baklýsing er á pinna 4 (3)
#define RESET 3 // Reset er á pinna 1 (4)
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC); // Skilgreining fyrir ILI9341 kóðasafn

// Svo skilgreinum við kassa og önnur form á skjánum.
#define FRAME_RFT_X 260
#define FRAME_RFT_Y 20
#define FRAME_RFT_W 60
#define FRAME_RFT_H 40

#define FRAME_LFT_X 20
#define FRAME_LFT_Y 20
#define FRAME_LFT_W 60
#define FRAME_LFT_H 40

#define FRAME_LRT_X 20
#define FRAME_LRT_Y 180
#define FRAME_LRT_W 60
#define FRAME_LRT_H 40

#define FRAME_RRT_X 240
#define FRAME_RRT_Y 180
#define FRAME_RRT_W 60
#define FRAME_RRT_H 40
// eða allavegana hingað.

// Skilgreinum MENU takka
#define MENU_X 110
#define MENU_Y 10
#define MENU_W 130
#define MENU_H 38

// Skilgreinum útlínur bíls.
#define FRAME_CAR_X 125
#define FRAME_CAR_Y 80
#define FRAME_CAR_W 80
#define FRAME_CAR_H 150

// dekk vinstra framan
#define LFT_X 110
#define LFT_Y 100
#define TIRE_W 30
#define TIRE_H 35

// dekk hægra framan
#define RFT_X LFT_X+FRAME_CAR_W
#define RFT_Y LFT_Y

// Dekk Vinstra megin aftan
#define LRT_X LFT_X
#define LRT_Y 180

// Dekk hægra megin aftan
#define RRT_X RFT_X
#define RRT_Y LRT_Y

// Skilgreinum hækka takka sem er þríhyrningur
#define INCREMENT_PRESSURE_X0 270
#define INCREMENT_PRESSURE_Y0 90
#define INCREMENT_PRESSURE_X1 240
#define INCREMENT_PRESSURE_Y1 130
#define INCREMENT_PRESSURE_X2 300
#define INCREMENT_PRESSURE_Y2 130

// Skilgreinum lækka takka sem er einnig þríhyrningur.
#define DECREMENT_PRESSURE_X0 50
#define DECREMENT_PRESSURE_Y0 130
#define DECREMENT_PRESSURE_X1 20
#define DECREMENT_PRESSURE_Y1 90
#define DECREMENT_PRESSURE_X2 80
#define DECREMENT_PRESSURE_Y2 90

// Skilgreinum liti
#define BLACK       0x0000        // Bakgrunnur
#define DARKGREEN   0x04E0        // Vöktun - var 0x3E0
#define C_INNDAELING        0x001F      // Inndæling (BLUE)
#define GREEN       0x07E0      /*   0, 255,   0 */
#define CYAN        0x07FF      /*   0, 255, 255 */
#define C_WARNING   0xF800      // RED
#define MAGENTA     0xF81F      /* 255,   0, 255 */
#define C_URHLEYPING      0xFFE0      // úRHLEYPIING (YELLOW)
#define WHITE       0xFFFF      // Valið dekk
#define C_MAELING   0xFD20      // Mæling (ORANGE)
#define PINK        0xF81F

// Skilgreinum global breytur
bool debug = true; // breyta í true fyrir debug

float selectedPressure = 0.00f; // Valinn þrýstingur.
float selectedPressure_LRT = 0.00f;
float selectedPressure_LFT = 0.00f;
float selectedPressure_RFT = 0.00f;
float selectedPressure_RRT = 0.00f;
float pressure_ALL = 0.00f; // Þrýstingur í kistu með öll dekk opin.
float pressure_LFT = 0.00f; //Breyta sem geymir þrýsting vinstra framdekks.
float pressure_RFT = 0.00f; // Breyta sem geymir þrýsting Hægra framdekks
float pressure_LRT = 0.00f; // Breyta sem geymir vinstra afturdekks
float pressure_RRT = 0.00f; // Breyta sem geymir hægra afturdekks.


unsigned long previousMillis = 0; // Teljari 1
unsigned long previousMillis1 = 0; // Teljari 2
unsigned long previousMillis2 = 0; // Teljari 3
unsigned long interval = 6000; // hve lengi á að bíða þar til athugað er aftur. 6s ~= 0.1psi
unsigned long interval1 = 600000; // interval1 er hugsað fyrir athugun á dekkjaþrýstingi, er 10mínútur.
unsigned long interval2 = 300000; // interval2 er hugsað fyrir athugun dekkjaþrýstings
uint16_t interval_ALL = 6000;
uint16_t interval_LRT = 6000; // Tími LRT
uint16_t interval_LFT = 6000;
uint16_t interval_RFT = 6000;
uint16_t interval_RRT = 6000;

uint8_t menuval = 0; // er menu valið eða ekki?
uint8_t selectedTire =0; // Hvaða dekk er valið.
uint16_t psi = 0; //
bool forval = false; // Er forval valið eða ekki?
//bool baklysingval = false; // Er valið að stilla baklýsingu? Er þetta notað einhverstaðar???
bool adjust = false; // Á að stilla eða á ekki að stilla?
bool adjustall = false; // Breyta sem segir forritinu að stilla öll dekk í einu.
bool manual = false; // Ef við erum í manual, þá er selectedpressure valinn fyrir hvert dekk fyrir sig
uint8_t tiretoken = 0; // Dekk sem heldur tokeninu ræður
uint8_t tireval = 0; // Valið dekk
uint8_t backlight_selected = 255; // Styrkur á baklýsingu
uint16_t timerTire = 0; //Hve oft við athugum þrýsting í dekkjum áður en við gefumst upp í bili.

// Skilgreinum öll föll
void updateValues(); // Við uppfærum öll gildi.
void drawTireSelection(); // Við teiknum valmynd fyrir dekkjaval
void drawMain(); // Við teiknum aðal útlit
void drawMenu(); // Við teiknum menu útlit
void drawForval(); // Við teiknum Forvals útlit
void warningCheck(); // Þegar eitthvað bilar.
void air_base_close(); // Lokum öllum lokum
void updateBaseValue(); // Lesum þrýsting af kistu
float readPressure(); // Skilgreinum fall til að lesa þrýsting
void readTires(); // Skilgreinum fall til að lesa þrýsting.
int tirePaint(int tire_colour, int tire); // til að teikna og lita dekk.
void read_RRT(); //Lesum þrýsting í hægra afturhjóli
void read_RFT(); // lesum þrýsting í hægra framhjóli
void read_LFT(); // Lesum þrýsting í vinstra framhjóli
void read_LRT(); // Lesum þrýsting í vinstra afturhjóli
float readTemp(); // Lesum hitanema
void adjustLRT(); // Jöfnum þrýsting í vinstra afturdekki
void adjustLFT(); // Jöfnum þrýsting í Vinstra framdekki
void adjustRFT(); // Jöfnum þrýsting í hægra Framdekki
void adjustRRT(); // Jöfnum þrýsting í hægra afturdekki
void adjustAllTires(); // Við stillum öll dekk í einu.
void calibrate(); // Calibrate lúppa sem er hugsuð fyrir upphafsstillingu, les CALIBRATE flaggið
void writeSelectedPressure(); // Skrifum þrýstingsval í EEPROM.
void toggleMenu(); // Litum menu takka grænan eða svartan eftir því hvort vöktun sé virk eður ei

// Hér eftir fylgja allar undirlykkjur

// Við veljum tíma fyrir interval
void timerSelector()
{
  // Hér þurfum við að bera saman selectedPressure og pressure
  // Byrjum á ALL
  if(((selectedPressure - pressure_ALL) > 10.00) && ((selectedPressure - pressure_ALL) < 20.00))
  {
    interval_ALL = TIMER10PSI; // 30 sekúndur
  }
  // Ef þrýstingsmunur á völdu gildi og raungildi er milli 5 og 10
  if(((selectedPressure - pressure_ALL) > 5.00) && ((selectedPressure - pressure_ALL) < 10.00))
  {
    interval_ALL = TIMER5PSI; // 10 sekúndur
  }
  if(((selectedPressure - pressure_ALL) > 1.00) && ((selectedPressure - pressure_ALL) < 5.00))
  {
    interval_ALL = TIMER1PSI; // 6 sekúndur
  }
  if(((selectedPressure - pressure_ALL) > 0.00) && ((selectedPressure - pressure_ALL) < 1.00))
  {
    interval_ALL = TIMER025PSI; // 3 sekúndur
  }

  // Næst LRT
  if(((selectedPressure_LRT - pressure_LRT) > 10.00) && ((selectedPressure_LRT - pressure_LRT) < 30.00))
  {
    interval_LRT = TIMER10PSI; // 30 sekúndur
  }
  // Ef þrýstingsmunur á völdu gildi og raungildi er milli 5 og 10
  if(((selectedPressure_LRT - pressure_LRT) > 5.00) && ((selectedPressure_LRT - pressure_LRT) < 10.00))
  {
    interval_LRT = TIMER5PSI; // 10 sekúndur
  }
  if(((selectedPressure_LRT - pressure_LRT) > 1.00) && ((selectedPressure_LRT - pressure_LRT) < 5.00))
  {
    interval_LRT = TIMER1PSI; // 6 sekúndur
  }
  if(((selectedPressure_LRT - pressure_LRT) > 0.00) && ((selectedPressure_LRT - pressure_LRT) < 1.00))
  {
    interval_LRT = TIMER025PSI; // 3 sekúndur
  }

  // Næst LFT
  if(((selectedPressure_LFT - pressure_LFT) > 10.00) && ((selectedPressure_LFT - pressure_LFT) < 20.00))
  {
    interval_LFT = TIMER10PSI; // 30 sekúndur
  }
  // Ef þrýstingsmunur á völdu gildi og raungildi er milli 5 og 10
  if(((selectedPressure_LFT - pressure_LFT) > 5.00) && ((selectedPressure_LFT - pressure_LFT) < 10.00))
  {
    interval_LFT = TIMER5PSI; // 10 sekúndur
  }
  if(((selectedPressure_LFT - pressure_LFT) > 1.00) && ((selectedPressure_LFT - pressure_LFT) < 5.00))
  {
    interval_LFT = TIMER1PSI; // 6 sekúndur
  }
  if(((selectedPressure_LFT - pressure_LFT) > 0.00) && ((selectedPressure_LFT - pressure_LFT) < 1.00))
  {
    interval_LFT = TIMER025PSI; // 3 sekúndur
  }

  // RFT
  if(((selectedPressure_RRT - pressure_RFT) > 10.00) && ((selectedPressure_RFT - pressure_RFT) < 20.00))
  {
    interval_RFT = TIMER10PSI; // 30 sekúndur
  }
  // Ef þrýstingsmunur á völdu gildi og raungildi er milli 5 og 10
  if(((selectedPressure_RFT - pressure_RFT) > 5.00) && ((selectedPressure_RFT - pressure_RFT) < 10.00))
  {
    interval_RFT = TIMER5PSI; // 10 sekúndur
  }
  if(((selectedPressure_RFT - pressure_RFT) > 1.00) && ((selectedPressure_RFT - pressure_RFT) < 5.00))
  {
    interval_RFT = TIMER1PSI; // 6 sekúndur
  }
  if(((selectedPressure_RFT - pressure_RFT) > 0.00) && ((selectedPressure_RFT - pressure_RFT) < 1.00))
  {
    interval_RFT = TIMER025PSI; // 3 sekúndur
  }

  // RRT
  if(((selectedPressure_RRT - pressure_RRT) > 10.00) && ((selectedPressure_RRT - pressure_RRT) < 20.00))
  {
    interval_RRT = TIMER10PSI; // 30 sekúndur
  }
  // Ef þrýstingsmunur á völdu gildi og raungildi er milli 5 og 10
  if(((selectedPressure_RRT - pressure_RRT) > 5.00) && ((selectedPressure_RRT - pressure_RRT) < 10.00))
  {
    interval_RRT = TIMER5PSI; // 10 sekúndur
  }
  if(((selectedPressure_RRT - pressure_RRT) > 1.00) && ((selectedPressure_RRT - pressure_RRT) < 5.00))
  {
    interval_RRT = TIMER1PSI; // 6 sekúndur
  }
  if(((selectedPressure_RRT - pressure_RRT) > 0.00) && ((selectedPressure_RRT - pressure_RRT) < 1.00))
  {
    interval_RRT = TIMER025PSI; // 3 sekúndur
  }

}

// Við skrifum vistuð gildi í EEPROM
void writeSelectedPressure()
{
  //EEPRON.put(ESERIALNUMBER,SERIALNUMBER); // Skrifum inn serial númer
  EEPROM.put(EPRESSURE,selectedPressure); //valinn þrýstingur
  EEPROM.put(EPRESSURE_LRT,selectedPressure_LRT); // Valinn þrýstingur per dekk
  EEPROM.put(EPRESSURE_LFT,selectedPressure_LFT);
  EEPROM.put(EPRESSURE_RFT,selectedPressure_RFT);
  EEPROM.put(EPRESSURE_RRT,selectedPressure_RRT);
}

// Stillum öll dekk í einu.
// Þetta þarf sennilega að bæta...
void adjustAllTires()
{
  //unsigned long millis() = millis(); //Uppfærum teljara
  if(((pressure_ALL*100-selectedPressure*100))>25) // þegar það er of mikill þrýstingur
  {
    tiretoken = 5; // Við höldum token til að stilla öll dekk
    digitalWrite(AIR_IN,OFF);
    digitalWrite(TIRE_LR,ON); // Opnum loka í dekk
    digitalWrite(TIRE_LF,ON);
    digitalWrite(TIRE_RF,ON);
    digitalWrite(TIRE_RR, ON);
    digitalWrite(AIR_OUT,ON); // Opnum fyrir loft út
    tirePaint(C_URHLEYPING,tiretoken); // Litum dekk fjólublátt

    if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
    {
      updateBaseValue(); // Uppfærum mælingu á kistu
      previousMillis2 = millis(); // Endurstillum teljarann
    }

    if(millis() - previousMillis > interval_ALL) // ef það er kominn tími á að mæla
    {
      read_LRT(); // Lesum þrýsting
      read_LFT();
      read_RFT();
      read_RRT();
      previousMillis = millis(); //endurstillum teljara
      updateValues();
      // Hérna myndum við vilja bera saman þrýsting á öllum dekkjum
      // Við breytum öllu í heiltölu í centiPSI, fyrir lesanleika
      uint16_t test_LRT = pressure_LRT*100;
      uint16_t test_LFT = pressure_LFT*100;
      uint16_t test_RFT = pressure_RFT*100;
      uint16_t test_RRT = pressure_RRT*100;

      // Reynist summa allra vera hærri en valins þrýstings.
      if((test_LRT + test_LFT + test_RFT + test_RRT)  > (selectedPressure*400))
      {
        // Þá leiðréttum við hvert dekk fyrir sig, og höldum svo áfram.
        adjustLRT(); // Stillum vinstra afturdekk
        adjustLFT(); // Stillum vinstra framdekk
        adjustRFT(); // Stillum hægra framdekk
        adjustRRT(); // Stillum hægra afturdekk
        tiretoken = 5; // Setjum okkur aftur í að stilla öll dekk
      }
      else
      pressure_ALL = pressure_LRT; // öll dekk hafa sama þrýsting.
    }
  } // Lækkun þrýstings fall lokar

      if((selectedPressure*100 - pressure_ALL*100)>25) // þegar það er of lítill þrýstingur
      {
        tiretoken = 5; // Við höldum token til að stilla öll dekk
        digitalWrite(AIR_OUT,OFF); // Verum viss um að loft sé ekki að fara út
        digitalWrite(TIRE_LR,ON); // Opnum loka í dekk
        digitalWrite(TIRE_LF,ON);
        digitalWrite(TIRE_RF,ON);
        digitalWrite(TIRE_RR,ON);
        digitalWrite(AIR_IN,ON); // Opnum fyrir loft inn
        tirePaint(C_INNDAELING,tiretoken);

        if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
        {
          updateBaseValue(); // Uppfærum mælingu á kistu
          previousMillis2 = millis(); // Endurstillum teljarann

        }

        if(millis() - previousMillis > interval_ALL) // Ef það er kominn tími til að mæla
        {
          air_base_close(); // Lokum kistu
          read_LRT(); // Lesum þrýsting
          read_LFT();
          read_RFT();
          read_RRT();
          updateValues();
          previousMillis = millis(); //endurstillum teljara

          // Hérna myndum við vilja bera saman þrýsting á öllum dekkjum
          // Við breytum öllu í heiltölu í milliPSI, fyrir lesanleika
          int test_LRT = pressure_LRT*100;
          int test_LFT = pressure_LFT*100;
          int test_RFT = pressure_RFT*100;
          int test_RRT = pressure_RRT*100;

          // Reynist summa allra vera lægri en valins þrýstings.
          if((selectedPressure*400) < (test_LRT + test_LFT + test_RFT + test_RRT))
          {
            // Þá leiðréttum við hvert dekk fyrir sig, og höldum svo áfram.
            adjustLRT(); // Stillum vinstra afturdekk
            adjustLFT(); // Stillum vinstra framdekk
            adjustRFT(); // Stillum hægra framdekk
            adjustRRT(); // Stillum hægra afturdekk
            tiretoken = 5; // Setjum okkur aftur í að stilla öll dekk
          }
          else
          pressure_ALL = pressure_LRT; // öll dekk hafa sama þrýsting.
        }
      }// Hækkun þrýstings fall lokar

    //Við athugum hvort við séum innan skekkjumarka. Sé svo þá stillum við hvert dekk fyrir sig.
    if(((pressure_ALL*100)-(selectedPressure*100))<=200 && (((selectedPressure*100) - (pressure_ALL*100))<=200 ))
    {
      air_base_close();
      timerTire = 0; // Núllstillum teljara
      tiretoken = 1; // Færum okkur í fyrsta dekk og stillum hvert fyrir sig.
    }
    //Við athugum hvort við séum innan skekkjumarka

}// Lokum adjustAll



//Stillum vinstra afturdekk
void adjustLRT()
{

  if(((pressure_LRT*100)-(selectedPressure*100))>25) // þegar það er of mikill þrýstingur
  {
    tiretoken = 1; // Við höldum token til að stilla þetta dekk
    digitalWrite(TIRE_LR,ON); // Opnum loka í dekk
    digitalWrite(AIR_OUT,ON); // Opnum fyrir loft út
    tirePaint(C_URHLEYPING,tiretoken); // Litum dekk fjólublátt
    // Mælum þrýsting á kistu
    if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
    {
      updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
      previousMillis2 = millis(); // Endurstillum teljarann
    }

    if((millis() - previousMillis) > interval_LRT) // ef það er kominn tími á að mæla
    {
      tiretoken = tiretoken+1; //Förum síðan í næsta dekk eftir þessa mælingu
      read_LRT(); // Lesum þrýsting
      updateValues();
      previousMillis = millis(); //endurstillum teljara
    }
  } // Lækkun þrýstings fall lokar

      if(((selectedPressure*100) - (pressure_LRT*100))>25) // þegar það er of lítill þrýstingur
      {
        tiretoken = 1; // Við höldum token til að stilla þetta dekk
        digitalWrite(TIRE_LR,ON); // Opnum loka í dekk
        digitalWrite(AIR_IN,ON); // Opnum fyrir loft inn
        tirePaint(C_INNDAELING,tiretoken);

        // Mælum þrýsting á kistu
        if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
        {
          updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
          previousMillis2 = millis(); // Endurstillum teljarann
        }

        // Ef það er kominn tími til að mæla
        if(millis() - previousMillis > interval_LRT)
        {
          tiretoken = tiretoken+1; //Förum síðan í næsta dekk eftir þessa mælingu
          digitalWrite(TIRE_LR,OFF); // Lokum loka
          delay(AIR_DELAY); // Hinkrum
          digitalWrite(AIR_OUT,OFF); // Lokum fyrir loft út
          delay(AIR_DELAY); // Hinkrum
          read_LRT(); // Lesum vinstra afturdekk
          updateValues(); // Uppfærum gildin
          previousMillis = millis(); // endurstillum teljarann
        }
      }// Hækkun þrýstings fall lokar

    //Við athugum hvort við séum innan skekkjumarka
    if(((pressure_LRT*100)-(selectedPressure*100))<=25 && (((selectedPressure*100) - (pressure_LRT*100))<=25 ))
    {
      air_base_close();
      timerTire = 0; // Núllstillum teljara
      tiretoken = 2; // Færum okkur í næsta dekk
    }
    //millis() += millis();
} //Lokum adjustLRT

//Við stillum vinstra framdekk
void adjustLFT()
{
    if(((pressure_LFT*100)-(selectedPressure*100))>25) // þegar það er of mikill þrýstingur
    {
      tiretoken = 2; // Við höldum token til að stilla þetta dekk
      digitalWrite(TIRE_LF,ON); // Opnum loka í dekk
      digitalWrite(AIR_OUT,ON); // Opnum fyrir loft út
      tirePaint(C_URHLEYPING,tiretoken); // Litum dekk fjólublátt
      // Mælum þrýsting á kistu
      if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
      {
        updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
        previousMillis2 = millis(); // Endurstillum teljarann
      }

      if(millis() - previousMillis > interval_LFT) // ef það er kominn tími á að mæla
      {
        tiretoken = 3; //Förum síðan í næsta dekk eftir þessa mælingu
        air_base_close(); // Lokum kistu
        read_LFT(); // Lesum þrýsting
        updateValues(); // Uppfærum gildi
        previousMillis = millis(); //endurstillum teljara
      }

    }// Lækkun þrýstings fall lokar

        if(((selectedPressure*100) - (pressure_LFT*100))>25) // þegar það er of lítill þrýstingur
        {
          tiretoken = 2; // Við höldum token til að stilla þetta dekk
          digitalWrite(TIRE_LF,ON); // Opnum loka í dekk
          digitalWrite(AIR_IN,ON); // Opnum fyrir loft inn
          tirePaint(C_INNDAELING,tiretoken);
          // Mælum þrýsting á kistu
          if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
          {
            updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
            previousMillis2 = millis(); // Endurstillum teljarann
          }
        }// Hækkun þrýstings fall lokar

        if(millis() - previousMillis > interval_LFT) // Ef það er kominn tími til að mæla
        {
          tiretoken = 3; //Förum síðan í næsta dekk eftir þessa mælingu
          air_base_close(); // Lokum kistu
          delay(100); // Hinkrum
          read_LFT(); // Lesum vinstra framdekk
          updateValues(); // Uppfærum gildin
          previousMillis = millis(); // endurstillum teljarann
        }

      //Við athugum hvort við séum innan skekkjumarka (0.25psi frá völdu gildi)
      if(((pressure_LFT*100)-(selectedPressure*100))<=25 && (((selectedPressure*100) - (pressure_LFT*100))<=25 ))
      {
        air_base_close(); // Lokum kistunni
        timerTire = 0; // Núllstillum teljara
        tiretoken = 3; // Færum okkur í næsta dekk
      }
}// Lokum adjustLFT

//Til að stilla hægra framdekk
void adjustRFT()
{
  {
    if(((pressure_RFT*100)-(selectedPressure*100))>25) // þegar það er of mikill þrýstingur
    {
      tiretoken = 3; // Við höldum token til að stilla þetta dekk
      digitalWrite(TIRE_RF,ON); // Opnum loka í dekk
      digitalWrite(AIR_OUT,ON); // Opnum fyrir loft út
      tirePaint(C_URHLEYPING,tiretoken); // Litum dekk fjólublátt
      // Mælum þrýsting á kistu
      if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
      {
        updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
        previousMillis2 = millis(); // Endurstillum teljarann
      }

      if(millis() - previousMillis > interval_RFT) // ef það er kominn tími á að mæla
      {
        tiretoken = 4; //Förum síðan í næsta dekk eftir þessa mælingu
        air_base_close(); // Lokum kistu
        read_RFT(); // Lesum þrýsting
        updateValues(); // Uppfærum gildi
        previousMillis = millis(); //endurstillum teljara
      }

    }// Lækkun þrýstings fall lokar

        if(((selectedPressure*100) - (pressure_RFT*100))>25) // þegar það er of lítill þrýstingur
        {
          tiretoken = 3; // Við höldum token til að stilla þetta dekk
          digitalWrite(TIRE_RF,ON); // Opnum loka í dekk
          digitalWrite(AIR_IN,ON); // Opnum fyrir loft inn
          tirePaint(C_INNDAELING,tiretoken);
          // Mælum þrýsting á kistu
          if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
          {
            updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
            previousMillis2 = millis(); // Endurstillum teljarann
          }
        }// Hækkun þrýstings fall lokar

        if(millis() - previousMillis > interval_RFT) // Ef það er kominn tími til að mæla
        {
          tiretoken = 4; //Förum síðan í næsta dekk eftir þessa mælingu
          air_base_close(); // Lokum kistu
          delay(100); // Hinkrum
          read_RFT(); // Lesum vinstra afturdekk
          updateValues(); // Uppfærum Gildin
          previousMillis = millis(); // endurstillum teljarann
        }

      //Við athugum hvort við séum innan skekkjumarka
      if(((pressure_RFT*100)-(selectedPressure*100))<=25 && (((selectedPressure*100) - (pressure_RFT*100))<=25 ))
      {
        air_base_close(); // Lokum kistu
        timerTire = 0; // Núllstillum teljara
        tiretoken = 4; // Færum okkur í næsta dekk
      }
    }
}// Lokum adjustRFT

//Til að stilla hægra afturdekk
void adjustRRT()
{
  {
    if(((pressure_RRT*100)-(selectedPressure*100))>25) // þegar það er of mikill þrýstingur
    {
      tiretoken = 4; // Við höldum token til að stilla þetta dekk
      digitalWrite(TIRE_RR,ON); // Opnum loka í dekk
      digitalWrite(AIR_OUT,ON); // Opnum fyrir loft út
      tirePaint(C_URHLEYPING,tiretoken); // Litum dekk fjólublátt
      // Mælum þrýsting á kistu
      if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
      {
        updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
        previousMillis2 = millis(); // Endurstillum teljarann
      }

      if(millis() - previousMillis > interval_RRT) // ef það er kominn tími á að mæla
      {
        tiretoken = 1; //Förum síðan í næsta dekk eftir þessa mælingu
        air_base_close(); // Lokum kistu
        read_RRT(); // Lesum þrýsting
        updateValues(); // Uppfærum gildi
        previousMillis = millis(); // endurstillum teljarann
      }

    }// Lækkun þrýstings fall lokar

        if(((selectedPressure*100) - (pressure_RRT*100))>25) // þegar það er of lítill þrýstingur
        {
          tiretoken = 4; // Við höldum token til að stilla þetta dekk
          digitalWrite(TIRE_RR,ON); // Opnum loka í dekk
          digitalWrite(AIR_IN,ON); // Opnum fyrir loft inn
          tirePaint(C_INNDAELING,tiretoken); //Litum dekk
          // Mælum þrýsting á kistu
          if(millis() - previousMillis2 > 1000) // Ef það er kominn tími til að mæla
          {
            updateBaseValue(); // Uppfærum mælingu á kistu (Þetta er experimental)
            previousMillis2 = millis(); // Endurstillum teljarann
          }
        }// Hækkun þrýstings fall lokar

        if(millis() - previousMillis > interval_RRT) // Ef það er kominn tími til að mæla
        {
          tiretoken = 1; //Förum síðan í næsta dekk eftir þessa mælingu
          air_base_close(); // Lokum kistu
          delay(100); // Hinkrum
          read_RRT(); // Lesum vinstra afturdekk
          updateValues(); // Uppfærum gildin
          previousMillis = millis(); // endurstillum teljarann
        }
      //Við athugum hvort við séum innan skekkjumarka
      if(((pressure_RRT*100)-(selectedPressure*100))<=25 && (((selectedPressure*100) - (pressure_RRT*100))<=25 ))
      {
        air_base_close(); // Lokum kistu
        timerTire = 0; // Núllstillum teljara
        tiretoken = 1; // Færum okkur í næsta dekk
      }
    }
}// Lokum adjustRRT

void read_LRT() // Lesum vinstra afturdekk
{
  tirePaint(C_MAELING,1); // Litum dekk rauðgult.
  air_base_close(); // Lokum kistunni
  digitalWrite(TIRE_LR,ON); // Opnum loka fyrir Vinstra afturdekk
  delay(AIR_DELAY); // hinkrum
  pressure_LRT = readPressure(); // Lesum þrýsting
  delay(100); // töf
  digitalWrite(TIRE_LR,OFF);
  warningCheck(); // Athugum hvort allt sé með felldu
  //previousMillis = millis(); // endurstillum teljarann
}

void read_LFT() // Lesa vinstra framdekk
{
  tirePaint(C_MAELING,2); // Litum dekk rauðgult meðan við mælum.
  air_base_close(); // Verum viss um að kista sé lokuð
  delay(AIR_DELAY); // Hinkrum í 300ms
  digitalWrite(TIRE_LF,ON); // Opnum fyrir dekk LF
  delay(AIR_DELAY); // töf meðan kista fyllist
  pressure_LFT = readPressure(); // Lesum þrýsting
  delay(100); // töf
  digitalWrite(TIRE_LF,OFF); // Lokum kistunni
  warningCheck(); // Athugum hvort allt sé með felldu
}

void read_RFT() // Lesa Hægra framdekk
{
  tirePaint(C_MAELING,3);
  air_base_close();
  delay(AIR_DELAY); // Hinkrum í 300ms
  digitalWrite(TIRE_RF,ON); // Opnum fyrir dekk
  delay(AIR_DELAY); // Töf á meðan kistan fyllist.
  pressure_RFT = readPressure(); // lesum þrýsting í dekki
  delay(100); // Töf
  digitalWrite(TIRE_RF,OFF); // Lokum kistunni
  warningCheck(); // Athugum hvort allt sé með felldu
}

void read_RRT() // Lesa hægra afturdekk
{
  tirePaint(C_MAELING,4);// Litum dekk rauðgult
  air_base_close(); // Lokum kistunni
  delay(AIR_DELAY); // Töf
  digitalWrite(TIRE_RR,ON); // Opnum fyrir dekk
  delay(AIR_DELAY); // Töf á meðan kistan fyllist.
  pressure_RRT = readPressure(); // Lesum þrýsting í dekki.
  delay(100); // Töf
  digitalWrite(TIRE_RR,OFF); // Lokum kistunni.
  warningCheck(); // Athugum hvort allt sé með felldu
}

void updateBaseValue() // Við uppfærum gildi á kistu
{
    // Hreinsum þar sem gildin eru á skjánum.
    tft.fillRect(120,60,240,18,BLACK); // Hreinsum gildi fyrir þrýsting kistu
    tft.fillRect(130,140,60,48,BLACK); // Hreinsum gildi fyrir valinn þrýsting burt.

    // Við skrifum mælt gildi á kistunni
    float basePressure = 0.0;
    basePressure = readPressure();
    pressure_ALL = basePressure; // þrýstingur úr öllum dekkjum
    tft.setTextSize(2);
    tft.setCursor(125,60);
    tft.println(basePressure);
    tft.setCursor(175,60);
    tft.println(" PSI");

    // ef það er kveikt á debug
    if(debug == true)
    {
      // Skrifum valið gildi á skjá
      tft.setTextSize(2); // Stillum textastærð á 2
      tft.setCursor(130,160); // Veljum staðsetningu
      //tft.println((pressure_ALL - selectedPressure)*-1); // Skrifum út gildið.
      //
      uint16_t sekundur = millis()/1000;
      tft.println(interval_LFT);
      tft.setCursor(130,140); // Veljum staðsetningu
      tft.println(tiretoken);
    }


  //  warningCheck(); //Athugum hvort viðvörun gildi á einhverju dekki
}

void updateValues() // Við uppfærum gildi
{
    // Hreinsum þar sem gildin eru á skjánum.
    //updateBaseValue();
    tft.setTextSize(2);
    tft.fillRect(20,170,60,50, BLACK); // Hreinsum gildið fyrir LRT
    tft.fillRect(20,20,60,50,BLACK); // Hreinsum gildið fyrir LFT
    tft.fillRect(240,20,60,50,BLACK); // Hreinsum gildið fyrir RFT
    tft.fillRect(240,170,60,50,BLACK); // Hreinsum gildið fyrir RRT
    //tft.fillRect(110,60,220,18,BLACK); // Hreinsum gildi fyrir hitastig

    // Skrifum mæld gildi.
    tft.setCursor(20,20); // Staðsetjum okkur v/m ofan
    if(selectedTire == 2)
    {
      tft.setTextColor(GREEN);
    }
    else
    {
      tft.setTextColor(WHITE);
    }
    tft.println(selectedPressure_LFT);
    tft.setCursor(20,40); // Erum fyrir neðan.
    tft.setTextColor(GREEN);
    tft.println(pressure_LFT); // Skrifum út gildi á staðsetningu

    tft.setCursor(240,20); // Staðsetjum okkur hægra megin að ofan
    if(selectedTire == 3)
    {
      tft.setTextColor(GREEN);
    }
    else
    {
      tft.setTextColor(WHITE);
    }
    tft.println(selectedPressure_RFT); // Valinn þrýstingur
    tft.setTextColor(GREEN);
    tft.setCursor(240,40); // Skrifum á RFT
    tft.println(pressure_RFT);


    tft.setCursor(20,170);
    if(selectedTire == 1)
    {
      tft.setTextColor(GREEN);
    }
    else
    {
      tft.setTextColor(WHITE);
    }
    tft.println(selectedPressure_LRT);
    tft.setTextColor(GREEN);
    tft.setCursor(20,190); // Skrifum á LRT
    tft.println(pressure_LRT);

    if(selectedTire == 4)
    {
      tft.setTextColor(GREEN);
    }
    else
    {
      tft.setTextColor(WHITE);
    }

    tft.setCursor(240,170);
    tft.println(selectedPressure_RRT);
    tft.setTextColor(GREEN);
    tft.setCursor(240,190); // Skrifum á RRT
    tft.println(pressure_RRT);

    // Skrifum menu textann
    tft.setTextColor(WHITE);
    tft.setTextSize(3); // Textastærð 3.
    tft.setCursor(130,20); // Setjum byrjunarreit
    tft.println("MENU"); // Skrifum Menu

    // Skrifum hitastig
    // Sleppum þessu - Hitastig innan í bíl skiptir eiginlega ekki máli og verður
    // Hvort eð er tekið úr í Version 2.0
    //float hitastig = 0.0;
    //hitastig = readTemp();
    //ft.setTextSize(2);
    //tft.setCursor(125,60);
    //tft.println(hitastig);
    //tft.setCursor(165,60);
    //tft.println("   C");

    warningCheck(); //Athugum hvort viðvörun gildi á einhverju dekki
}

//Fallið sem vaktar dekkin og framkvæmir mælingu
void tireMon()
{

}
void drawTireSelection()
{
  tft.fillScreen(BLACK); // Viljum vera viss um að skjárinn sé tómur.
  //tft.drawRect(FRAME_CAR_X, FRAME_CAR_Y-30, FRAME_CAR_W,FRAME_CAR_H, WHITE); // Teiknum kassann utan um bíl.
  tireval = 5; // Við erum í valmynd fyrir dekkjaval

  tft.setTextSize(3); // Textastærð
  tft.setCursor(50,110);
  tft.println(" Veldu dekk");
  tft.drawRect(20,30,80,50,WHITE); // Teiknum vinstra framdekk
  tft.setCursor(25,45);
  tft.println(" VF");
  tft.drawRect(220,30,80,50,WHITE); // Teiknum hægra framdekk
  tft.setCursor(225,45);
  tft.println(" HF");
  tft.drawRect(220,160,80,50,WHITE); // Teiknum hægra afturdekk
  tft.setCursor(225,175);
  tft.println(" HA");
  tft.drawRect(20,160,80,50,WHITE);// Teiknum vinstra afturdekk
  tft.setCursor(25,175);
  tft.println(" VA");
  //Teiknum fram og aftur
  tft.setCursor(120,45);
  tft.drawRect(100,30,120,50,WHITE);
  tft.println("Fram");
  tft.drawRect(100,160,120,50,WHITE); // Teiknum ramma
  tft.setCursor(120,175);
  tft.println("Aftur");

  // Til baka takki.
  tft.setCursor(110,220);
  tft.setTextSize(2);
  tft.println("Til baka");
} // Lokum valmynd 3

void drawMain()
{
    tft.fillScreen(BLACK); // Viljum vera viss um að skjárinn sé tómur.
    menuval = 0; // Slökkvum á menuvali
    tft.drawRect(FRAME_CAR_X, FRAME_CAR_Y, FRAME_CAR_W,FRAME_CAR_H, WHITE); // Teiknum kassann utan um bíl.


    // Tveir þríhyrningar fyrir hækka og lækka takkana.
    tft.fillTriangle(INCREMENT_PRESSURE_X0, INCREMENT_PRESSURE_Y0, INCREMENT_PRESSURE_X1, INCREMENT_PRESSURE_Y1, INCREMENT_PRESSURE_X2, INCREMENT_PRESSURE_Y2, WHITE);
    tft.fillTriangle(DECREMENT_PRESSURE_X0, DECREMENT_PRESSURE_Y0, DECREMENT_PRESSURE_X1, DECREMENT_PRESSURE_Y1, DECREMENT_PRESSURE_X2, DECREMENT_PRESSURE_Y2, WHITE);
    toggleMenu(); // litum Menu kassann grænan eða aflitum hann eftir því sem við á.
    updateValues(); // Uppfærum gildi

    if(adjust == false && tiretoken == 0)
    {
      for(int a=4;a>0;a--)
      {
        tirePaint(GREEN,a);
      }
    }

}// drawMain fall lokar

void drawMenu()
{
  /* Valmöguleikarnir eru:
     * Stilla - Fer í auto ham
     * 2 Forval - Opnar valmynd fyrir fyrirfram ákveðin gildi
     * 3 Stilla dekk - Opnar valmynd til að stilla hvert dekk fyrir sig.
     * 4 Mæla - Mælir öll dekk.
     * 5 Stillingar - Baklýsing, og fleira
     *   Til baka
     */
    tft.fillScreen(BLACK); // Hreinsum skjáinn fyrir Menu
    tft.setTextSize(2); // Stillum á stærð 2
    //Teiknum box fyrir takkana
    tft.drawRect(100,3,MENU_W+20,MENU_H, WHITE); // Teiknum Stilla box
    tft.drawRect(100,MENU_H+3,MENU_W+20,MENU_H, WHITE); // Teiknum Forval box
    tft.drawRect(100,2*MENU_H+3,MENU_W+20,MENU_H, WHITE); // Teiknum Baklysing box
    tft.drawRect(100,3*MENU_H+3,MENU_W+20,MENU_H, WHITE); // Teiknum Maela box
    tft.drawRect(100,4*MENU_H+3,MENU_W+20,MENU_H, WHITE); // Teiknum Til baka box
    tft.drawRect(100,5*MENU_H+3,MENU_W+20,MENU_H,WHITE); //

    tft.setTextSize(2);
    tft.setCursor(120,20);
    tft.println(" Stilla");
    tft.setTextSize(2);
    tft.setCursor(100,MENU_H+20);
    tft.println(" Forval");
    tft.setTextSize(2);
    tft.setCursor(100,2*MENU_H+20);
    tft.println(" Handvirkt");
    tft.setTextSize(2);
    tft.setCursor(100,3*MENU_H+20);
    tft.println(" Maela");
    tft.setTextSize(2);
    tft.setCursor(100,4*MENU_H+20);
    tft.println(" Stillingar");
    tft.setTextSize(2);
    tft.setCursor(100,5*MENU_H+20);
    tft.println(" Til baka");
} // Lokum DrawMenu

// Teiknum forvalsvalmynd. 2,4,8,12,20,28 psi eru í boði.
void drawForval()
{
  tft.setTextSize(2); // Stillum á stærð 2
  tft.fillScreen(ILI9341_BLACK); // Hreinsum skjáinn fyrir Forval

  //tft.drawRect(20,MENU_H,MENU_W,MENU_H, WHITE); // Teiknum 2psi ramma
  tft.setCursor(20,30); // Stillum byrjunarreit
  tft.print("  2 PSI"); // Prentum texta

  //tft.drawRect(20,2*MENU_H,MENU_W,MENU_H, WHITE); //  Teiknum 4psi ramma
  tft.setCursor(20,80); // Stillum byrjunarreit
  tft.print("  4 PSI"); //  Prentum texta

  //tft.drawRect(20,3*MENU_H+10,MENU_W,MENU_H,WHITE); // Teiknum 8psi ramma
  tft.setCursor(20,130);
  tft.print("  8 PSI"); //prentum texta

  //tft.drawRect(180,2*MENU_H+10,MENU_W,MENU_H,WHITE); // 8psi rammi

  // Þarf að vinna að þessu ....
  //tft.drawRect(20,3*MENU_H+10,MENU_W,MENU_H, WHITE); // Teiknum 20psi ramma
  tft.setCursor(180,30);
  tft.print("  12 PSI");

  tft.setCursor(180,80);  // Stillum byrjunarreit
  tft.print("  20 PSI"); // Prentum texta
  //tft.drawRect(180,3*MENU_H+10,MENU_W,MENU_H, WHITE); // Teiknum 28psi ramma
  tft.setCursor(180,130); // Stillum byrjunarreit
  tft.print("  28 PSI"); // Prentum texta
  tft.drawRect(100,5*MENU_H+10,MENU_W,MENU_H, WHITE); // Teiknum til baka ramma
  tft.setCursor(100,5*MENU_H+20); // Stillum byrjunarreit
  tft.println(" Til baka "); // Prentum texta
}

// Lokum öllum lokum í kistu
void air_base_close()
{
    digitalWrite(AIR_OUT,OFF); // Lokum aflestunarloka
    digitalWrite(AIR_IN,OFF); // Lokum inndælingarloka
    digitalWrite(TIRE_LR,OFF); // Lokum vinstra afturhjóli
    digitalWrite(TIRE_LF,OFF); // Lokum vinstra framhjóli
    digitalWrite(TIRE_RF,OFF); // lokum hægra framhjóli
    digitalWrite(TIRE_RR,OFF); // Lokum hægra afturhjóli.
    digitalWrite(AIR_OUT,ON); // Tæmum kistu
    delay(AIR_DELAY);
    digitalWrite(AIR_OUT,OFF); // Lokum út
}

// Veljum lit fyrir dekk og teiknum ferhyrning
int tirePaint(int tire_colour, int tire)
{
  //Ef dekkið er nr 1, Vinstra afturdekk
  if(tire == 1)
  {
    tft.fillRect(LRT_X, LRT_Y, TIRE_W, TIRE_H, BLACK); // Hreinsum vinstra afturhjól
    tft.fillRect(LRT_X, LRT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum vinstra afturhjól
  }
  // Ef dekkið er nr 2, vinstra framdekk
  if(tire == 2)
  {
    tft.fillRect(LFT_X, LFT_Y, TIRE_W, TIRE_H, BLACK); // Hreinsum vinstra framhjól
    tft.fillRect(LFT_X, LFT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum vinstra framhjól

  }
  // Ef dekkið er nr 3, hægra framdekk
  if(tire == 3)
  {
    tft.fillRect(RFT_X, RFT_Y, TIRE_W, TIRE_H, BLACK); // Hreinsum hægra framhjól
    tft.fillRect(RFT_X, RFT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum hægra framhjól
  }
  // Ef dekkið er nr 4, hægra afturdekk
  if(tire == 4)
  {
    tft.fillRect(RRT_X, RRT_Y, TIRE_W, TIRE_H, BLACK); // Hreinsum hægra afturhjól
    tft.fillRect(RRT_X, RRT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum hægra afturhjól
  }
  // Ef dekkið er nr 5 þá erum við að stilla öll dekk.
  if(tire == 5)
  {
      tft.fillRect(LRT_X, LRT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum vinstra afturhjól
      tft.fillRect(LFT_X, LFT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum vinstra framhjól
      tft.fillRect(RFT_X, RFT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum hægra framhjól
      tft.fillRect(RRT_X, RRT_Y, TIRE_W, TIRE_H, tire_colour); // Teiknum hægra afturhjól
  }

  return tire_colour; // Skilum lit, ekki notað í núverandi útgáfu en hugsað fyrir framtíðarviðbætur

}

// Fall til að stilla baklýsingu
int backlightAdjust(int val)
{
    analogWrite(BACKLIGHT,val); // Breytum birtustigi
    return val; // Skilum völdu gildi

}

void warningCheck()
{
    /*
     * Hér kemur fall virkjar viðvörun á skjá þegar eitthvað fer úrskeiðis.
     * T.d. ef eitt dekk verður skyndilega loftlaust á ferð.
     * * ef eitt eða fleiri dekk eru skyndilega komin undir 0.5psi
     * Á skjánum mun birtast hvert dekkjanna er sýnir hættumerki ef viðvörun fer í gildi.
     */
  if(menuval == 0) // Ef við erum ekki í Menu
  {
      // Síðan þurfum við að tjekka hvaða dekk er í veseni.

      // Ef þrýstingur er undir 0.5psi eða yfir 35 psi í vinstra afturdekki
      if((pressure_LRT*100 < 25) || (pressure_LRT > 35))
      {
        tirePaint(C_WARNING,1); // Við litum dekkið rautt
      }
      // Ef ástand lagast, dekk er yfir 0.5psi og undir 35 psi
      if((pressure_LRT > 0.5) && (pressure_LRT < 35))
      {
        tirePaint(GREEN,1); // Litum dekkið grænt
      }

      // Ef vinstra framhjól er loftlaust eða of hár þrýstingur
      if((pressure_LFT*100 < 25) || (pressure_LFT > 35))
      {
        tirePaint(C_WARNING,2); // Teiknum vinstra framhjól
      }
      // Ef ástand lagast
      if((pressure_LFT*100 > 25) && (pressure_LFT < 35))
      {
        tirePaint(GREEN,2); // Litum dekk grænt
      }

      // Ef hægra framhjól er loftlaust eða of hár þrýstingur
      if((pressure_RFT*100 < 25) || (pressure_RFT > 35))
      {
        tirePaint(C_WARNING,3); // Teiknum hægra framhjól
      }
      if((pressure_RFT*100 > 25) && (pressure_RFT < 35))
      {
        tirePaint(GREEN,3); // Litum dekk grænt
      }
      // Ef hægra afturhjól er loftlaust eða of hár þrýstingur
      if((pressure_RRT*100 < 25 || pressure_RRT > 35))
      {
        tirePaint(C_WARNING,4); // Teiknum hægra afturhjól rautt
      }
      if((pressure_RRT*100 > 25) && (pressure_RRT < 35))
      {
        tirePaint(GREEN,4);  // Litum dekk grænt
      }
  }

} // Lokum warningCheck fallinu

// Lesum þrýsting á skynjara og skilum gildi
float readPressure()
{
    /* Skynjarinn er MPX5700 og les max 100psi
     *     100psi MAX pressure
     * 6,4mV/0.145psi
     * Við viljum lesa eins nákvæmt og hægt er, en þó ekki birta gögnin nema með 0.25psi nákvæmni.
     * 11mV/0.250psi
     */

  int val = analogRead(P_SENSOR); // Lesum gildi
  psi = val*100; // færum það upp um 100
  psi = map(psi,4100,94000,0,10000); // efri/neðri mörk

  float pressure = 0.00f; // Færum í fljótandi gildi
  pressure = psi;
  pressure = pressure/100.00f; // Umreiknum

  if(pressure < 0 || pressure > 650) // Komum í veg fyrir "buffer overflow"
  {
    pressure = 0;
  }
  pressure = 2.00; // test!
  return pressure;
} //readPressure fall lokar

// Lesum hitastig og skilum gildum.
float readTemp()
{
    float temperature, read_positive, read_negative;

    // Read the positive and negative signal from LM35

    read_positive = analogRead(TEMP_POSITIVE);
    read_negative = analogRead(TEMP_NEGATIVE);

    // *magic* (No just kidding). We divide our signal voltage with 1024 and multiply that by 5000mV.
    read_positive = ((read_positive / 1024.0) * 5000) / 10.0;
    read_negative = ((read_negative / 1024.0) * 5000) / 10.0;

    temperature = read_positive - read_negative;
    return temperature;
} // ReadTemp fall lokar

void test()
{
  // Hleypum úr 10s per dekk
  digitalWrite(AIR_OUT,ON);
  delay(2000);
  digitalWrite(TIRE_LR,ON);
  delay(10000);
  digitalWrite(TIRE_LR,OFF);
  delay(1000);
  digitalWrite(TIRE_LF,ON);
  delay(10000);
  digitalWrite(TIRE_LF,OFF);
  delay(1000);
  digitalWrite(TIRE_RF,ON);
  delay(10000);
  digitalWrite(TIRE_RF,OFF);
  delay(1000);
  digitalWrite(TIRE_RR,ON);
  delay(10000);
  digitalWrite(TIRE_RR,OFF);
  delay(1000);
  digitalWrite(AIR_IN,ON); // Prófum dælu
  delay(5000);
  digitalWrite(AIR_OUT,OFF);
  delay(1000);
  // Dælum í 10s í hvert dekk

  digitalWrite(TIRE_LR,ON);
  delay(10000);
  digitalWrite(TIRE_LR,OFF);
  delay(1000);
  digitalWrite(TIRE_LF,ON);
  delay(10000);
  digitalWrite(TIRE_LF,OFF);
  delay(1000);
  digitalWrite(TIRE_RF,ON);
  delay(10000);
  digitalWrite(TIRE_RF,OFF);
  delay(1000);
  digitalWrite(TIRE_RR,ON);
  delay(10000);
  digitalWrite(TIRE_RR,OFF);
  delay(1000);
  digitalWrite(AIR_IN,OFF);
  delay(15000);

}
// Ef við viljum lita Menu hnappinn
void toggleMenu()
{
  if(adjust == true)
  {
    tft.fillRect(MENU_X, MENU_Y, (MENU_W-20), MENU_H, DARKGREEN);    // Teiknum menu takka dökkgrænan ef við erum í stillingu
  }
  else
  tft.drawRect(MENU_X, MENU_Y, (MENU_W-20), MENU_H, WHITE);
}

// Setjum inn upphafskóða
void setup() { // Setup hefst.

// Skilgreinum eftirfarandi sem útganga
    pinMode(AIR_IN, OUTPUT);
    pinMode(TIRE_RR, OUTPUT);
    pinMode(TIRE_RF, OUTPUT);
    pinMode(TIRE_LF, OUTPUT);
    pinMode(TIRE_LR, OUTPUT);
    pinMode(AIR_OUT, OUTPUT);
    pinMode(BACKLIGHT, OUTPUT); // Baklýsing PWM
    pinMode(RESET,OUTPUT); // Reset á skjá



// Skilgreinum eftirfarandi sem innganga.
    pinMode(IGNITION, INPUT_PULLUP); //Notum innra pull-up viðnám
// Næst koma skilgreiningar fyrir TFT skjá
  delay(5000); // Gefum þéttum og öðru tíma til að hlaðast upp svo skjárinn komi rétt upp.
  digitalWrite(RESET,LOW); // Endurræsing á skjá.
  //delay(10); // töf
  digitalWrite(RESET,HIGH); // Ræsum skjá
  delay(100); // Töf
  tft.begin(); // Virkjum skjáinn
  tft.fillScreen(ILI9341_BLACK); // Hreinsum skjáinn og skrifum svartan bakgrunn.
  tft.setRotation(1); // Við stillum skjá í landscape ham.

  // Hér lesum við úr minni eldri stillingar
  backlight_selected = EEPROM.read(EBACKLIGHT);
  EEPROM.get(EPRESSURE,selectedPressure); // Lesum þrýsting úr minni
  EEPROM.get(EPRESSURE_LRT,selectedPressure_LRT); // Lesum þrýsting úr minni
  EEPROM.get(EPRESSURE_LFT,selectedPressure_LFT); // Lesum þrýsting úr minni
  EEPROM.get(EPRESSURE_RFT,selectedPressure_RFT); // Lesum þrýsting úr minni
  EEPROM.get(EPRESSURE_RRT,selectedPressure_RRT); // Lesum þrýsting úr minni

  // Boot skilaboð
  delay(500); //Gefum skjá tækifæri á að ræsa sig.
  backlightAdjust(255); // Kveikjum á baklýsingu.
  tft.setTextSize(2); // Stærð eitt fyrir texta í booti
  tft.setTextColor(GREEN); // Grænn texti fyrir smá nostalgíu
  //tft.print("Bunadur nr: ");
  //tft.println(SERIALNUMBER);
  tft.print("ATACS Version: "); // Útgáfa
  tft.println(VERSION); // Utgáfa
  tft.println(BUILDDATE); //Dagsetning útgáfu
  tft.println("Hofundur:");
  tft.println("Samuel Hjaltalin");
  tft.println("https://ulfraf.space");
  warningCheck(); // teiknum dekk rauð fyrir mælingu
  read_LRT(); // Lesum vinstra afturdekk
  read_LFT(); // Lesum vinstra framdekk
  read_RFT(); // Lesum hægra framdekk
  read_RRT(); // Lesum hægra afturdekki
  tft.fillScreen(BLACK); // Hreinsum skjá.
  tft.setTextColor(WHITE); // Breytum textanum yfir í hvítt
  drawMain(); // Teiknum grunn útlit.
}//Void Setup lokar

//Aðalfall

void loop()
{
  timerSelector(); // Ákveðum lengd á tímanum sem þarf að dæla í

    //millis() = millis(); // Tími fyrir teljara 0
    //unsigned long millis1 = millis(); // Tími fyrir teljara 1
    //unsigned long millis()2 = millis(); // Tími fyrir teljara 2
    backlightAdjust(backlight_selected); // Við kveikjum á skjá.

  // sækjum hnit sem ýtt er á
  TSPoint p = ts.getPoint();

  // Ef þrýst er á skjáinn og er innan min/max marka
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE)
  {
  p.x = map(p.x, TS_MINY, TS_MAXY, 0, tft.height()); // möppum lesið gildi á X ás með min/max þrýstingi á skjá
  p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.width()); //möppum lesið gildi á y ás með min/max þrýstingi á skjá
  int y = tft.height() - p.x; // Y hnit eftir því hvernig skjár snýr
  int x = p.y;

  // Hér erum við í aðalvalmynd.
  // Ef ýtt er á lækka þrýsting örina.
  if(menuval == 0 && (x > 10) && (x<100)) // Athugum staðsetningu á x ásnum
  {
    if((y>50) && y< 150) // Athugum staðsetningu á y ásnum.
    {
      adjust = false; // Við hættum að stilla
      if((selectedPressure < 6) && (selectedPressure > 0)) // Ef þrýstingur er undir 6 psi en yfir 0psi
      {
        selectedPressure = selectedPressure - 0.25; // Þá lækkum við um 0.25 psi
        selectedPressure_LRT = selectedPressure_LRT - 0.25;
        selectedPressure_LFT = selectedPressure_LFT - 0.25;
        selectedPressure_RFT = selectedPressure_RFT - 0.25;
        selectedPressure_RRT = selectedPressure_RRT - 0.25;
        delay(500); // töf til að koma í veg fyrir að hoppa of hratt á milli stiga.

      }
      if(selectedPressure >= 6) // ef þrýstingur er yfir 6
      {
        selectedPressure = selectedPressure - 1.0; // Lækkum við um 1psi í einu.
        selectedPressure_LRT = selectedPressure_LRT - 1.0;
        selectedPressure_LFT = selectedPressure_LFT - 1.0;
        selectedPressure_RFT = selectedPressure_RFT - 1.0;
        selectedPressure_RRT = selectedPressure_RRT - 1.0;
        delay(500); // Töf til að koma í veg fyrir að hoppa of hratt milli stiga.
      }
      adjust = true; // byrjum aftur að stilla
    tiretoken = 0;
    toggleMenu();
    updateValues(); // Uppfærum gildin á skjá.
    }

  }
  // Ef ýtt er á hækka þrýsting örina.
  if(menuval == 0 && (x > 250) && (x<320) && (selectedPressure < 35)) // Athugum staðsetningu á x ás og hvort þrýstingur sé undir 35psi.
  {
    if((y>50) && y< 150)
    {
      adjust = false;
      if(selectedPressure >= 6) // sé þrýstingur yfir 6psi hækkum við um 1psi í skrefi
      {
        selectedPressure = selectedPressure + 1.0; // Við hækkum gildið um 1psi
        selectedPressure_LRT = selectedPressure_LRT + 1.0;
        selectedPressure_LFT = selectedPressure_LFT + 1.0;
        selectedPressure_RFT = selectedPressure_RFT + 1.0;
        selectedPressure_RRT = selectedPressure_RRT + 1.0;
        delay(500); // Hinkrum í smá stund svo hann hækki sig ekki upp of hratt
      }

      if((selectedPressure < 35 && (selectedPressure < 6))) // Sé þrýstingurinn undir 6psi hækkum við um 0.25psi í hverju skrefi.
      {
        if(selectedTire == 0)
        {
          selectedPressure = selectedPressure + 0.25; // bætum 0,25psi við valið gildi
          selectedPressure_LRT = selectedPressure_LRT + 0.25;
          selectedPressure_LFT = selectedPressure_LFT + 0.25;
          selectedPressure_RFT = selectedPressure_RFT + 0.25;
          selectedPressure_RRT = selectedPressure_RRT + 0.25;
          delay(500); // töf svo við hækkum ekki of hratt upp
        }

        if(selectedTire == 1)
        {
          selectedPressure_LRT = selectedPressure_LRT +0.25; // Bætum við 0.25psi
          delay(500);
        }
        if(selectedTire == 2)
        {
          selectedPressure_LFT = selectedPressure_LFT + 0.25;
          delay(500);
        }
        if(selectedTire == 3)
        {
          selectedPressure_RFT = selectedPressure_RFT + 0.25;
          delay(500);
        }
        if(selectedTire == 4)
        {
          selectedPressure_RRT = selectedPressure_RRT + 0.25;
        }
      }
      tiretoken = 0;
      adjust = true; // förum aftur að stilla
      toggleMenu();
      updateValues(); // Uppfærum gildi á skjá.

    }

  }
// Hér lýkur aðalvalmynd.
//Ef við veljum LRT
  if((menuval == 0) && (x>20) && (x<100))
  {
    if((y>180)&&(y<240))
    {
      if(selectedTire != 1) // Ef dekk LRT er ekki valið.
      {
        tft.fillRect(0,180,50,40, BLACK); // Hreinsum gildið fyrir LRT
        selectedTire = 1; // Þá veljum við það.
        selectedPressure_LRT = 20; // Test!
        updateValues(); // uppfærum gildi
      }
    }
  }

  if((menuval == 0) && (x>20) && (x<100))
  {
    if((y>20)&&(y<60))
    {
      selectedTire = 2; // Veljum LFT
      updateValues(); // uppfærum gildi
    }

  // Menu hluti byrjar.
  }
    if(menuval == 0  && (x >MENU_X) && (x<(MENU_X+MENU_W))) // Ef ýtt er á x hnit Menu
    {
      if((y>MENU_Y) && (y <= (MENU_Y + MENU_H))) // ef ýtt er á Y hnit menu
      {

        /* Valmöguleikarnir eru:
       * Stilla - Fer í auto ham
       * 2 Forval - Opnar valmynd fyrir fyrirfram ákveðin gildi
       * 3 Stilla dekk - Opnar valmynd til að stilla hvert dekk fyrir sig.
       * 4 Mæla - Mælir öll dekk.
       * 5 Stillingar - Baklýsing, og fleira
       *   Til baka
       */
       drawMenu(); // Teiknum Menu útlit

      menuval = 1; // Þá er menuval 1 sem heldur okkur í Menu valglugganum
      x = 0; // Hreinsum X ásinn svo við hoppum ekki beint í annað
      y = 0; // Hreinsum Y ásinn svo við hoppum ekki beint í annað
      //delay(500); // 500ms töf svo við hoppum ekki beint í annað
    }

  } // Lokum Menu lykkju

    // Ef valið er að stilla dekk
    if(menuval == 1  &&  (x > MENU_X) && (x < MENU_X+MENU_W))
    {
      if(menuval == 1 && (y>0) && (y<40)) // Ef ýtt er á stilla
      {
        if(adjust == true) // Ef við erum nú þegar að stilla þá hættum við
        {
          adjust = false; // Breytum gildi
          tiretoken = 0; // Ekkert dekk heldur tokeni
          menuval = 0;
          air_base_close(); // Gætum þess að kistan sé lokuð
        }
        else
        {
          adjust = true; // Setjum adjust gildið í true svo forritið byrji að stilla
          writeSelectedPressure(); // Skrifum valinn þrýsting í EEPROM.
          tiretoken = 0; // ekkert dekk heldur tokeni
          menuval = 0; // og við förum úr valmynd.
          delay(50); // Töf
        }
        drawMain(); // Teiknum aðalvalmynd
        x = 0; // Hreinsum X ásinn svo við hoppum ekki beint í annað
        y = 0; // Hreinsum Y ásinn svo við hoppum ekki beint í annað
      } // Lokum stilla lykkju
    }
    //Forval
    if((menuval == 1)  &&  (x > MENU_X) && (x < MENU_X+MENU_W)) // Ef við erum á takkanum
    {
      if((y>40) && (y < 80)) // Ef Y ásinn fellur á Forval
      {
        //delay(500); // Hinkrum í 500ms til að skynja snertingu.
        menuval = 2; // Tveir er fyrir forval
        drawForval(); // Teiknum forvals valmynd
        delay(500);//  smá töf
        x = 0; // Núllstillum X ás
        y = 0; // Núllstillum Y ás
      }
    }
    // Forval
    // Ef ýtt er vinstra megin á skjáinn
    if((menuval == 2)  &&  (x > 20) && (x < 150)) // Ef við erum á takkanum
    {
      if((y>10) && (y<60)) // Ef það er valið 2 PSI forval.
      {
        delay(500); // Smá töf
        selectedPressure = 2.00; // Setjum valinn þrýsting í 4psi
        selectedPressure_LFT = 2.00; //
        selectedPressure_LRT = 2.00;
        selectedPressure_RFT = 2.00;
        selectedPressure_RRT = 2.00;
        adjust = true;
        writeSelectedPressure(); // Skrifum í minni
        tiretoken = 5; // við stillum öll dekk í einu
        menuval = 0; // förum úr menu
        drawMain(); // teiknum aðalvalmynd.
        previousMillis = millis(); // endursetjum teljara
      } // Lokum if setningu
      if(y>80 && y<130) // Ef það er valið 4 PSI forval
      {
        delay(500); // Smá töf
        selectedPressure = 4.00; // setjum valinn þrýsting í 4 psi
        selectedPressure_LRT = 4.00; //
        selectedPressure_LFT = 4.00;
        selectedPressure_RFT = 4.00;
        selectedPressure_RRT = 4.00;

        adjust = true; // Stillum dekk
        writeSelectedPressure(); // Skrifum í minni
        tiretoken = 5; // Stillum öll dekk
        menuval = 0; // förum úr menu
        drawMain(); // Teiknum aðalvalmynd
        previousMillis = millis(); // endursetjum teljara
      } // Lokum if setningu
      if((y>130) && (y<160)) // Ef 8 psi eru valin
      {
        delay(500); // Smá töf
        selectedPressure = 8.00; // Setjum valinn þrýsting á öll dekk í 8psi
        selectedPressure_LRT = 8.00;
        selectedPressure_LFT = 8.00;
        selectedPressure_RFT = 8.00;
        selectedPressure_RRT = 8.00;
        adjust = true;
        writeSelectedPressure(); // Skrifum í minni
        tiretoken = 5; // Stillum öll dekk
        menuval = 0; // Förum úr menu
        drawMain(); // Teiknum aðalvalmynd
        previousMillis = millis(); // endursetjum teljara
      }
    }
    // Ef ýtt er hægra megin á skjáinn (8,12,20,28 PSI)
    if(menuval == 2 && x> 160 && x < 320) // Ef við erum á takkanum
    {
      if(y>20 && y<80) // og við erum á takkanum á y ásnum
      {
        delay(500); // Smá töf
        selectedPressure = 12.00; // Valinn þrýstingur er 8 psi
        selectedPressure_LFT = 12.00; //
        selectedPressure_LRT = 12.00;
        selectedPressure_RFT = 12.00;
        selectedPressure_RRT = 12.00;
        adjust = true; // Stillum dekk
        writeSelectedPressure(); // Skrifum í minni
        tiretoken = 5; // stillum öll dekk í einu
        menuval = 0; // Við förum úr menui
        drawMain(); // Teiknum aðalvalmynd
      }
      if(y>80 && y<130) // og við erum á takkanum á y ásnum
      {
        delay(500); // smá töf
        selectedPressure = 20.00; // Þrýstingur er 28psi
        selectedPressure_LFT = 20; //
        selectedPressure_LRT = 20;
        selectedPressure_RFT = 20;
        selectedPressure_RRT = 20;
        adjust = true; // Stillum dekk
        writeSelectedPressure(); // Skrifum í minni
        tiretoken = 5;
        menuval = 0; // Förum úr menu
        drawMain(); // Teiknum aðalvalmynd
      }
      if(y>130 && y< 160)
      {
        delay(500);
        selectedPressure = 28.00;
        selectedPressure_LRT = 28.00;
        selectedPressure_LFT = 28.00;
        selectedPressure_RFT = 28.00;
        selectedPressure_RRT = 28.00;
        selectedPressure = 28.00;
        adjust = true;
        writeSelectedPressure();
        tiretoken = 5;
        menuval = 0;
        drawMain();
      }
      x = 0; // Núllstillum X ás
      y = 0; // Núllstillum Y ás
    } // Lokum forvals lykkju


    // Til að stilla hvert dekk fyrir sig
    if((menuval == 1)  &&  (x > MENU_X) && (x < MENU_X+MENU_W)) // Ef við erum á takkanum
    {
      if((menuval == 1) && (y>80) && (y<120))
      {
        menuval = 3; // Festum okkur í þessari valmynd.
        // Teiknum upp dekkin
        drawTireSelection(); // Teiknum valmynd fyrir dekkjaval

      }
    }
    // Ef við veljum VA
    if((menuval == 3) && (x > 20) && (x < 100))
    {
      if((y > 160) && (y < 240))
      {
        // Tveir þríhyrningar fyrir hækka og lækka takkana.
        menuval = 31;
        tft.fillScreen(BLACK); // Hreinsum skjá
        tft.fillTriangle(120, 60, 170, 20, 220, 60, WHITE); // Teiknum efri þríhryning.
        tft.fillTriangle(120,120,170,160,220,120,WHITE); // Teiknum neðri þríhyrning.
        tft.setCursor(0,80); // Stillum á miðjuna
        tft.setTextSize(2);
        tft.print(" Vinstra Afturdekk: ");
        tft.println(selectedPressure_LRT);
        // Teiknum tilbaka takka
        tft.drawRect(100,5*MENU_H+10,MENU_W+20,MENU_H, WHITE); // Teiknum ramma fyrir tilbaka
        tft.setTextSize(2); // Textastærð í 2
        tft.setCursor(100,5*MENU_H+20); // Stillum hvar við viljum byrja að teikna
        tft.println(" Til baka "); // Prentum texta
        delay(500); // Töf
      }
    }

    // Ef við veljum VF
    if((menuval == 3) && (x > 20) && (x < 100))
    {
      if((y > 10) && (y < 120))
      {
        // Tveir þríhyrningar fyrir hækka og lækka takkana.
        menuval = 32;
        tft.fillScreen(BLACK); // Hreinsum skjá
        tft.fillTriangle(120, 60, 170, 20, 220, 60, WHITE); // Teiknum efri þríhryning.
        tft.fillTriangle(120,120,170,160,220,120,WHITE); // Teiknum neðri þríhyrning.
        tft.setCursor(0,80); // Stillum á miðjuna
        tft.setTextSize(2);
        tft.print(" Vinstra Framdekk: ");
        tft.println(selectedPressure_LFT);
        // Teiknum tilbaka takka
        tft.drawRect(100,5*MENU_H+10,MENU_W+20,MENU_H, WHITE); // Teiknum ramma fyrir tilbaka
        tft.setTextSize(2); // Textastærð í 2
        tft.setCursor(100,5*MENU_H+20); // Stillum hvar við viljum byrja að teikna
        tft.println(" Til baka "); // Prentum texta
        delay(500); // Töf
      }
    }
    // Ef við veljum HF
    if((menuval == 3) && (x > 220) && (x < 320))
    {
      if((y > 10) && (y < 120))
      {
        // Tveir þríhyrningar fyrir hækka og lækka takkana.
//        tft.fillTriangle(100,110,160,40,200,110,WHITE)
        menuval = 33;
        tft.fillScreen(BLACK); // Hreinsum skjá
        tft.fillTriangle(120, 60, 170, 20, 220, 60, WHITE); // Teiknum efri þríhryning.
        tft.fillTriangle(120,120,170,160,220,120,WHITE); // Teiknum neðri þríhyrning.
        tft.setCursor(0,80); // Stillum á miðjuna
        tft.setTextSize(2);
        tft.print(" Haegra Framdekk: ");
        tft.println(selectedPressure_LFT);
        // Teiknum tilbaka takka
        tft.drawRect(100,5*MENU_H+10,MENU_W+20,MENU_H, WHITE); // Teiknum ramma fyrir tilbaka
        tft.setTextSize(2); // Textastærð í 2
        tft.setCursor(100,5*MENU_H+20); // Stillum hvar við viljum byrja að teikna
        tft.println(" Til baka "); // Prentum texta
        delay(500); // Töf
      }
    }
    // Ef við veljum hægra afturdekk
    if((menuval == 3) && (x > 220) && (x < 320))
    {
      if((y > 180) && (y < 220))
      {
        // Tveir þríhyrningar fyrir hækka og lækka takkana.
        tft.fillScreen(BLACK); // Hreinsum skjá
        menuval = 34; // Við erum í undirmenu af 3
        tft.fillTriangle(120, 60, 170, 20, 220, 60, WHITE); // Teiknum efri þríhryning.
        tft.fillTriangle(120,120,170,160,220,120,WHITE); // Teiknum neðri þríhyrning.
        tft.setCursor(0,80); // Stillum á miðjuna
        tft.setTextSize(2);
        tft.print("  Haegra Afturdekk: ");
        tft.println(selectedPressure_LFT);
        // Teiknum tilbaka takka
        tft.drawRect(100,5*MENU_H+10,MENU_W+20,MENU_H, WHITE); // Teiknum ramma fyrir tilbaka
        tft.setTextSize(2); // Textastærð í 2
        tft.setCursor(100,5*MENU_H+20); // Stillum hvar við viljum byrja að teikna
        tft.println(" Til baka "); // Prentum texta
        delay(500); // Töf
      }
    }

    // Hérna lesum við input fyrir handvirkar stillingar.
    // Vinstra afturdekk
    if((menuval == 31) && (x > 20) && (x < 100))
    {
      if((y > 140) && (y < 160))
      {
        //drawMain();
      }
      if((y>160) && (y<240))
      {

      }
    }

    // Vinstra framdekk
    if((menuval == 32) && (x > 20) && (x < 100))
    {
      if((y > 40) && (y < 80))
      {
        drawMain();
      }
      if((y>160) && (y<240))
      {

      }
    }
/*
    // Hægra framdekk
    if((menuval == 33) && (x > 20) && (x < 100))
    {
      if((y > 40) && (y < 80))
      {
        drawMain();
      }
      if((y>160) && (y<240))
      {

      }
    }

    // Hægra afturdekk
    if((menuval == 34) && (x > 20) && (x < 100))
    {
      if((y > 40) && (y < 80))
      {
        drawMain();
      }
      if((y>160) && (y<240))
      {

      }
    }

*/



// Til að mæla dekk
    if((menuval == 1)  &&  (x > MENU_X) && (x < MENU_X+MENU_W)) // Ef við erum á takkanum
    {
      if((menuval == 1) && (y>120) && (y<160)) // Ef ýtt er á maela
      {
        menuval = 0; // Förum úr menu
        adjust = 0; // Hættum að stilla
        drawMain(); // Teiknum aðalvalmynd
        read_LRT(); // Lesum vinstra afturdekk
        //updateValues(); // Uppfærum gildi
        read_LFT(); // Lesum vinstra framdekk
        //updateValues(); // Uppfærum gildin
        read_RFT(); // lesum hægra framdekk
        //updateValues(); // Uppfærum gildin
        read_RRT(); // Lesum hægra afturdekk
        updateValues(); // Uppfærum gildi
        previousMillis1 = millis(); // Endurstillum teljara svo hann mæli ekki strax aftur

      } // Lokum Mæla  lykkju
    }

//stillingar
    if((menuval == 1)  &&  (x > MENU_X) && (x < MENU_X+MENU_W)) // Ef við erum á takkanum
    {
      if((menuval == 1) && (y>180) && (y<200))
      {
        menuval = 5; // Við förum í stillingar
        //menuval = 4; // Segjum forritinu að við séum með menu baklýsing
        tft.fillScreen(BLACK); // Hreinsum skjá
        // Búum til örvatakka
        // Tveir þríhyrningar fyrir hækka og lækka takkana.
        tft.fillTriangle(INCREMENT_PRESSURE_X0, INCREMENT_PRESSURE_Y0, INCREMENT_PRESSURE_X1, INCREMENT_PRESSURE_Y1, INCREMENT_PRESSURE_X2, INCREMENT_PRESSURE_Y2, WHITE);
        tft.fillTriangle(DECREMENT_PRESSURE_X0, DECREMENT_PRESSURE_Y0, DECREMENT_PRESSURE_X1, DECREMENT_PRESSURE_Y1, DECREMENT_PRESSURE_X2, DECREMENT_PRESSURE_Y2, WHITE);
        // Sýnum núverandi gildið á skjá
        tft.setCursor(145,100); // Staðsetjum hvar við viljum teikna gildið
        tft.setTextSize(3); // Höfum textann í stærð 3
        tft.println(backlight_selected/10); // Skrifum gildið á skjá

        // Búum til tilbaka takka

        //tft.drawRect(100,5*MENU_H+10,MENU_W+20,MENU_H, WHITE); // Teiknum ramma fyrir tilbaka
        tft.setTextSize(2); // Textastærð í 2
        tft.setCursor(100,5*MENU_H+20); // Stillum hvar við viljum byrja að teikna
        tft.println(" Til baka "); // Prentum texta
        delay(500); // Töf
      } // Lokum baklýsingar hluta
    }


        // Ef ýtt er á lækka birtu örina.
        if(menuval == 5 && (x > 10) && (x<100) && (backlight_selected > 5)) // Athugum staðsetningu á x ásnum
        {
          if((y>50) && y< 150) // Athugum staðsetningu á y ásnum.
          {
            backlight_selected = backlight_selected - 25; // Þá lækkum við um 25 gildi
            delay(500); // töf til að koma í veg fyrir að hoppa of hratt á milli stiga.
            backlightAdjust(backlight_selected);
            // Sýnum gildið á skjá
            tft.fillRect(145,100,80,40,BLACK); // Hreinsum eldra gildi
            tft.setCursor(145,100); // Staðsetjum hvar við viljum teikna gildið
            tft.setTextSize(3); // Höfum textann í stærð 3
            tft.println(backlight_selected/10); // Skrifum gildið á skjá
          }
        }
        // Ef ýtt er á Hækka birtu örina.
        if(menuval == 5 && (x > 250) && (x<320) && (backlight_selected < 255)) // Athugum staðsetningu á x ás og hvort þrýstingur sé undir 35psi.
        {
          if((y>50) && y< 150)
          {
            backlight_selected = backlight_selected + 25; // Hækkum um 25 gildi
            delay(500); // Hinkrum í smá stund svo hann hækki sig ekki upp of hratt
            backlightAdjust(backlight_selected); //stillum birtu
            // Sýnum gildið á skjá
            tft.fillRect(145,100,80,40,BLACK); // Hreinsum eldra gildi
            tft.setCursor(145,100); // Staðsetjum hvar við viljum teikna gildið
            tft.setTextSize(3); // Höfum textann í stærð 3
            tft.println(backlight_selected/10); // Skrifum gildið á skjá
          }
      }

  // Til baka úr menu eða úr undir-menu í menu.
  if((menuval > 0) &&  (x > MENU_X) && (x < MENU_X+MENU_W)) // Ef við erum á takkanum
  {
      if((menuval == 1) && (y>200) && (y<240)) // Fyrir til baka
      {
        menuval = 0; // setjum gildið í 0 og þá skrifar hann hefðbundinn skjá á skjáinn.
        drawMain(); // Sennilega er betra að skrifa bara hefðbundinn skjá á til að losna við töfina.
      //  warningCheck(); // Athugum hvort ekki sé í lagi með dekk
      }
      // Ef við vorum í forvali
      if((menuval == 2) && (y>200) && (y<240))
      {
        drawMenu();
        menuval = 1; // Förum aftur í Menu
        delay(500);
      }
      // Ef við vorum að stilla hvert dekk fyrir sig
      if((menuval == 3) && (y>220) && (y<240))
      {
        drawMenu(); // Teiknum menu
        menuval = 1; // Höldum okkur í menu.
        delay(500);
      }
      if((menuval >30) && (menuval <35) && (y>200) && (y<240))
      {
        drawTireSelection(); // Teiknum valmynd fyrr handvirka stillingu.
        delay(500);
        menuval = 3; //
      }
      // Ef við vorum að stilla baklýsingu
      if((menuval == 5) && (y>200) && (y<240))
      {
          drawMenu(); // Teiknum menu.
          EEPROM.write(EBACKLIGHT,backlight_selected); // Geymum núverandi baklýsingu í EEPROM
          delay(100); // Töf
          menuval = 1;
      }
    }
  }

    // Er kominn tími til að mæla dekk? 10 mín ef við erum ekki í stillingu/vöktun
    // Tökum út adjust == false til prufu.
    // Prófum að athuga hvort við erum yfir 20 psi.
      if(menuval == 0 && tiretoken == 0 && selectedPressure < 20) // Ef við erum ekki í menu og erum ekki að stilla ákveðið dekk
      {

        if((millis() - previousMillis1) > interval1) // Athugum hve langt er liðið frá síðustu uppfærslu gilda
        {
          read_LRT(); // Lesum vinstra afturdekk
          updateValues(); // uppfærum gildi
          //read_LFT(); // Lesum vinstra framdekk
          updateValues(); // uppfærum gildi
          read_RFT(); // Lesum hægra framdekk
          updateValues(); // Uppfærum gildi
          //read_RRT(); // Lesum hægra afturdekk
          updateValues(); // Lesum gildi.
          //warningCheck(); // Athugum hvort eitthvað dekk sé í veseni.
          previousMillis1 = millis(); // Endurstillum teljara
        }
      }//Lokum athugunarfalli


      if(manual == false) // Ef við erum ekki með kerfið stillt á manual.
      {
        // Ef við erum ekki í menu og erum að vakta dekkin, en ekki að stilla stakt dekk

        if(menuval == 0 && adjust == true && tiretoken == 0)
        {
          uint16_t test_LRT = pressure_LRT*100;
          uint16_t test_LFT = pressure_LFT*100;
          uint16_t test_RFT = pressure_RFT*100;
          uint16_t test_RRT = pressure_RRT*100;
          // Reynist summa allra vera hærri en valins þrýstings.
          if(((test_LRT + test_LFT) - (test_RFT + test_RRT) <= 100))
          {
            // Þá leiðréttum við öll dekk í einu
            tiretoken = 5; // Setjum okkur aftur í að stilla öll dekk
          }
        }
        // Ef við erum ekki í menu og viljum stilla Vinstra afturdekk
        if(menuval == 0 && adjust == true && (tiretoken == 0 || tiretoken == 1))
        {
          adjustLRT(); // svo stillum við vinstra afturdekk
        }//Stillifall fyrir Vinstra afturdekk lokar

        if(menuval == 0 && adjust == true && (tiretoken == 0 || tiretoken == 2))
        {
          adjustLFT(); // athugum hvort stilla þurfi vinstra framdekk
        }
        // Ef við erum ekki í menu og það þarf að stilla hægra framdekk
        if(menuval == 0 && adjust == true && (tiretoken == 0 || tiretoken == 3))
        {
          adjustRFT();
        }
        // Ef við erum ekki í menu og það þarf að stilla hægra afturdekk
        if(menuval == 0 && adjust == true && (tiretoken == 0 || tiretoken == 4))
        {
          adjustRRT();
        }
        // Ef við erum ekki í menu og það þarf að stilla öll dekk
        if(menuval == 0 && adjust == true && (tiretoken == 0 || tiretoken == 5))
        {
          adjustAllTires();
        }
      }



} // Lokum void loop lykkju
