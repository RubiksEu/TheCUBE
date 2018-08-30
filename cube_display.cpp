
#include "cube_display.h"
#include "defines.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


extern volatile int Encoder_position_menu;
extern volatile bool enter_menu;
extern volatile bool select_button;

Adafruit_SSD1306 display(OLED_RESET);
char oled_terminal[8][25];
unsigned char curr_line = 0;

void initDisplay() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
}

void clearDisplay() {
  display.clearDisplay();
  display.display();
}

void clear_OLED_terminal() {
  unsigned char cnt;
  for (cnt=0;cnt<0;cnt++) { strcpy(oled_terminal[cnt], ""); };
  curr_line = 0;
  display.clearDisplay();
  display.display();
};


void println_on_OLED(char *what) {
  unsigned char cnt;
  char line[21];
  for (cnt=0;cnt<21;cnt++){
    line[cnt]=what[cnt];
  }

  line[21] = 0x00; // to have proper string ending

  if (curr_line<=7) {
    strcpy(oled_terminal[curr_line], line);
    curr_line++;
  }
  else { // scroll everything...
    for (cnt=0;cnt<7;cnt++) { strcpy(oled_terminal[cnt], oled_terminal[cnt+1]); };
    strcpy(oled_terminal[7], line);
  }

  //--- DISPLAY terminal
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  for (cnt=0;cnt<8;cnt++) { display.println(oled_terminal[cnt]); }; // display all lines
  display.display();
}


void initSplashScreen() {
  display.clearDisplay();
  display.display();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(14,0);
  display.println("CUBE 4.1 R2");
  display.setTextSize(2);
  display.setCursor(14,20);
  display.println("LOADING!");
  display.setTextSize(1);
  display.setCursor(14,52);
  display.println("Firmware:");
  display.display();
}

void updateSplashScreen(char* percentage) {
  corner_display(percentage);
}

void corner_display(const char* what) {
  display.fillRect(76, 46, 128, 64, WHITE);
  display.display();
  display.setTextSize(2);
  display.setTextColor(BLACK, WHITE);
  display.setCursor(78,48);
  display.println(what);
  display.setTextColor(WHITE);
  display.display();

};

void menuDisplay (const char* nr, const char* title) {
    display.clearDisplay();
    display.fillRect(0, 0, 32, 64, WHITE);
    display.drawRect(32, 0, (128-32), 64, WHITE);
    display.display();
    display.setTextSize(3);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(7,16);
    display.println(nr);
    display.setTextSize(1);
    display.setCursor(5,46);
    display.println(title);
    display.display();
};
//===========================================================================

void accMenuDisplay(char* xacc, char* yacc, char* zacc)
{
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y);
  display.print("X-Acc:");

  display.setCursor(
    MENU_START_POS_X_VALUE,
    MENU_START_POS_Y);
  display.println(xacc);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*2);
  display.print("Y-Acc:");

  display.setCursor(
    MENU_START_POS_X_VALUE,
    MENU_START_POS_Y + NEW_DISPLAY_LINE_1*2);
  display.println(yacc);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*4);
  display.print("Z-Acc:");

  display.setCursor(
    MENU_START_POS_X_VALUE,
    MENU_START_POS_Y + NEW_DISPLAY_LINE_1*4);
  display.println(zacc);

  display.display();

}

void actMenuDisplay(struct menuItem *act_menu, void (*actuate)())
{
  short x_indent_circle = MENU_START_POS_X + 5;
  short y_indent_circle = MENU_START_POS_Y + 14;
  short y_indent_text = MENU_START_POS_Y + 11;
  short y_indent_nl = 10;
  short y_indent_nl_text = 10;
  short circle_size = 3;
  static bool blink = false;

  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y);
  display.println("PRESS TO SELECT");

  do {
    for (int x=0; x < ACTUATORS_TO_BE_DISPLAYED; x++) {
       DEBUG_STREAM.println (act_menu[x].name);
       DEBUG_STREAM.println (act_menu[x].selected);

      if ((x == Encoder_position_menu) && (enter_menu == true)) { // if menu entry / selection blinking effect
        if (blink) {
          display.fillCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, WHITE);
        }
        else {
          display.fillCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, BLACK);
          display.drawCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, WHITE);
        }
      }
      else { // otherwise only fill the cirlce when the entry is really selected
        if (act_menu[x].selected) {
            display.fillCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, WHITE);
        }
        else {
            display.fillCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, BLACK);
            display.drawCircle(x_indent_circle, y_indent_circle + y_indent_nl*x, circle_size, WHITE);
        }
      }

      display.setCursor(x_indent_circle+10,y_indent_text + y_indent_nl_text*x);
      display.println(act_menu[x].name);
    }

    if ((select_button == true) && (enter_menu == true)) {
      DEBUG_STREAM.print ("menu updated");
      DEBUG_STREAM.println (Encoder_position_menu );
      act_menu[Encoder_position_menu].selected = !act_menu[Encoder_position_menu].selected;
    }

    blink = !blink;
    select_button = false;

    display.display();
    (*actuate)();
    delay (100);
  } while (enter_menu == true);


}

void networkMenuDisplay (
  char* IMEI,
  char* IMSI,
  char* Operator
)
{
  display.setTextSize(1);
  display.setTextColor(WHITE, BLACK);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y);
  display.print("IMEI");

  display.setCursor(
    MENU_START_POS_X,
    MENU_START_POS_Y + NEW_DISPLAY_LINE_1);
  display.println(IMEI);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*2);
  display.print("IMSI");

  display.setCursor(
    MENU_START_POS_X,
    MENU_START_POS_Y + NEW_DISPLAY_LINE_1*3);
  display.println(IMSI);

  display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*4);
  display.println(Operator);

  display.display();
};


// Display the main page
//---------------------------------------------------------------------------
void mainMenuDisplay (
  char* temperature,
  char* humidity,
  char* luminance,
  char* battery,
  char* sound)
  {
    display.setTextSize(1);
    display.setTextColor(WHITE, BLACK);

    display.setCursor(MENU_START_POS_X, MENU_START_POS_Y);
    display.print("Temp");

    display.setCursor(
      MENU_START_POS_X_VALUE,
      MENU_START_POS_Y);
    display.println(temperature);

    display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1);
    display.print("Hum");

    display.setCursor(
      MENU_START_POS_X_VALUE,
      MENU_START_POS_Y + NEW_DISPLAY_LINE_1);
    display.println(humidity);

    display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*2);
    display.print("Lum");

    display.setCursor(
      MENU_START_POS_X_VALUE,
      MENU_START_POS_Y + NEW_DISPLAY_LINE_1*2);
    display.println(luminance);

    display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*3);
    display.print("Bat");

    display.setCursor(
      MENU_START_POS_X_VALUE,
      MENU_START_POS_Y + NEW_DISPLAY_LINE_1*3);
    display.println(battery);

    display.setCursor(MENU_START_POS_X, MENU_START_POS_Y + NEW_DISPLAY_LINE_1*4);
    display.print("Sound");

    display.setCursor(
      MENU_START_POS_X_VALUE,
      MENU_START_POS_Y + NEW_DISPLAY_LINE_1*4);
    display.println(sound);

    display.display();
}
