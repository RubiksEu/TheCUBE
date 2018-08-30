

#ifndef CUBE_DISPLAY
#define CUBE_DISPLAY



void initDisplay();

void clearDisplay();

void clear_OLED_terminal();

void println_on_OLED(char *what);

void initSplashScreen();

void updateSplashScreen(char* percentage);

void corner_display(const char* what);;

void menuDisplay (const char* nr, const char* title);

void accMenuDisplay(char* xacc, char* yacc, char* zacc);

void actMenuDisplay(struct menuItem *act_menu, void (*actuate)());

void networkMenuDisplay (char* IMEI, char* IMSI, char* Operator);

void mainMenuDisplay (char* temperature, char* humidity, char* luminance, char* battery, char* sound);

#endif
