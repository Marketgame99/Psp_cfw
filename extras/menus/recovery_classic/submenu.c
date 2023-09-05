#include <pspsdk.h>
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <systemctrl.h>

#include <main.h>
#include <list.h>
#include <plugins.h>

extern ARKConfig* ark_config;
extern CFWConfig config;
extern List plugins;

#define SCREEN_WIDTH 58
#define SCREEN_HEIGHT 33 

#define printf pspDebugScreenPrintf

char* ark_settings_options[] = {
    (char*)"Disabled",
    (char*)"Always",
    (char*)"Game",
    (char*)"UMD/ISO",
    (char*)"Homebrew",
    (char*)"PS1",
    (char*)"XMB",
    (char*)"Launcher"
};

#define N_OPTS sizeof(ark_settings_options)/sizeof(ark_settings_options[0])

char* ark_settings_boolean[] = {
    (char*)"Off",
    (char*)"On"
};

char* ark_settings_infernocache[] = {
    (char*)"Off",
    (char*)"LRU",
    (char*)"RR"
};

struct {
    int max;
    int* value;
    char* name;
    char** opts;
} settings_items[] =
{
    { N_OPTS, &(config.usbcharge), "USB Charge", ark_settings_options },
    { N_OPTS, &(config.overclock), "Overclock", ark_settings_options },
    { N_OPTS, &(config.powersave), "PowerSave", ark_settings_options },
    { N_OPTS, &(config.defaultclock), "Balanced Energy Mode", ark_settings_options },
    { 2, &(config.launcher), "Autoboot Launcher", ark_settings_boolean },
    { 2, &(config.disablepause), "Disable Pause on PSP Go", ark_settings_boolean },
    { N_OPTS, &(config.highmem), "Force Extra Memory", ark_settings_options },
    { N_OPTS, &(config.mscache), "Memory Stick Speedup", ark_settings_options },
    { 3, &(config.infernocache), "Inferno Cache", ark_settings_infernocache },
    { N_OPTS, &(config.oldplugin), "Old Plugin Support on PSP Go", ark_settings_options },
    { 2, &(config.skiplogos), "Skip Sony Logos", ark_settings_boolean },
    { 2, &(config.hidepics), "Hide PIC0 and PIC1", ark_settings_boolean },
    { 2, &(config.hibblock), "Prevent hibernation deletion on PSP Go", ark_settings_boolean },
    { 2, &(config.hidemac), "Hide MAC Address", ark_settings_boolean },
    { 2, &(config.hidedlc), "Hide DLC", ark_settings_boolean },
    { N_OPTS, &(config.noled), "Turn off LEDs", ark_settings_options },
    { 2, &(config.noumd), "Disable UMD Drive", ark_settings_boolean },
};

#define N_SETTINGS (sizeof(settings_items)/sizeof(settings_items[0]))

static settings_to_text(char** names, char** states){
    for (int i=0; i<N_SETTINGS; i++){
        names[i] = settings_items[i].name;
        states[i] = settings_items[i].opts[*(settings_items[i].value)];
    }
}

static int plugins_to_text(char** paths, char** states, int dir){
    int ret = 0;
    for (int i=0; i<plugins.count; i++){
        Plugin* plugin = plugins.table[i];
        if (plugin->name != NULL){
            if (dir == ret){
                paths[ret] = plugin->path;
            }
            else{
                paths[ret] = strrchr(plugin->path, '/') + 1;
            }
            states[ret++] = (plugin->active)? "On" : "Off";
        }
    }
    return ret;
}

static void draw_submenu(char* header, char** options, char** states, int size, int dir){
    int start = 0;
    int osize = size;
    if (size > (SCREEN_HEIGHT/2)-3){
        size = (SCREEN_HEIGHT/2)-3;
    }

    if (dir >= size){
        start = dir;
    }

    pspDebugScreenSetXY(0, 1);
    pspDebugScreenSetTextColor(0xFFD800);
    printf("********************************************************************");

    pspDebugScreenSetXY(0, 2);
    printf(header);
    pspDebugScreenSetXY(0, 3);
    printf("*                                                                  *");

    for (int i=0; i<size; i++){
        if (start+i >= osize) break;
        pspDebugScreenSetXY(0, 4 + 2*i);
        char tmp[70];
        strcpy(tmp, "* ");
        if (dir == start+i){
            strcat(tmp, "> ");
        }
        strcat(tmp, options[start+i]);

        int len = strlen(tmp);
        int padding = 55 - len;
        for (int j=0; j<padding; j++) tmp[len+j] = ' ';
        strcpy(tmp+len+padding, states[start+i]);

        len = strlen(tmp);
        padding = 67 - len;
        for (int j=0; j<padding; j++) tmp[len+j] = ' ';
        tmp[len+padding] = '*';
        tmp[len+padding+1] = 0;
        printf(tmp);

        pspDebugScreenSetXY(0, 5 + 2*i);            
        printf("*                                                                  *");
    }

    // ADD SIDE BORDERS
    for (int i=pspDebugScreenGetY(); i<SCREEN_HEIGHT; i++) {
        pspDebugScreenSetXY(0, i);
        printf("*                                                                  *");
    }

    // BOTTOM BORDER
    pspDebugScreenSetXY(0, 33);
    printf("********************************************************************");
}

void plugins_submenu(){

    SceCtrlData pad;
    int dir = 0;

    char* header = "* Plugins Manager                                                  *";
    char** paths = malloc(sizeof(char*)*plugins.count);
    char** states = malloc(sizeof(char*)*plugins.count);

    int size = plugins_to_text(paths, states, dir);

    draw_submenu(header, paths, states, size, dir);

	while(1) {

        sceDisplayWaitVblankStart();

        sceCtrlPeekBufferPositive(&pad, 1);
		
		// CONTROLS
		if(pad.Buttons & PSP_CTRL_DOWN) {
            sceKernelDelayThread(200000);
			dir++;
			if(dir>=size) dir = 0;

            size = plugins_to_text(paths, states, dir);
            draw_submenu(header, paths, states, size, dir);
		}
		if(pad.Buttons & PSP_CTRL_UP) {
            sceKernelDelayThread(200000);
			dir--;
			if(dir<0) dir = size-1;
            
            size = plugins_to_text(paths, states, dir);
            draw_submenu(header, paths, states, size, dir);
		}
		if((pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_LEFT | PSP_CTRL_RIGHT))) {
            sceKernelDelayThread(200000);
            
            Plugin* plugin = plugins.table[dir];
            plugin->active = !plugin->active;

            size = plugins_to_text(paths, states, dir);
            draw_submenu(header, paths, states, size, dir);
        }
        if((pad.Buttons & PSP_CTRL_TRIANGLE)) {
            sceKernelDelayThread(200000);
            break;
        }
        
	}

    free(paths);
    free(states);

}

void settings_submenu(){
    SceCtrlData pad;
    int dir = 0;

    char* header = "* Custom Firmware Settings                                         *";
    char** paths = malloc(sizeof(char*)*N_SETTINGS);
    char** states = malloc(sizeof(char*)*N_SETTINGS);

    int size = N_SETTINGS;
    settings_to_text(paths, states);

    draw_submenu(header, paths, states, size, dir);

	while(1) {

        sceDisplayWaitVblankStart();

        sceCtrlPeekBufferPositive(&pad, 1);
		
		// CONTROLS
		if(pad.Buttons & PSP_CTRL_DOWN) {
            sceKernelDelayThread(200000);
			dir++;
			if(dir>=size) dir = 0;

            draw_submenu(header, paths, states, size, dir);
		}
		if(pad.Buttons & PSP_CTRL_UP) {
            sceKernelDelayThread(200000);
			dir--;
			if(dir<0) dir = size-1;
            
            draw_submenu(header, paths, states, size, dir);
		}
        if(pad.Buttons & PSP_CTRL_LEFT) {
            sceKernelDelayThread(200000);
			int* value = settings_items[dir].value;
            int max = settings_items[dir].max;
            *value = (*value) - 1;
			if(*value<0) *value = max-1;
            
            settings_to_text(paths, states);
            draw_submenu(header, paths, states, size, dir);
		}
        if(pad.Buttons & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_RIGHT)) {
            sceKernelDelayThread(200000);
			int* value = settings_items[dir].value;
            int max = settings_items[dir].max;
            *value = (*value) + 1;
			if(*value>=max) *value = 0;
            
            settings_to_text(paths, states);
            draw_submenu(header, paths, states, size, dir);
		}
        if((pad.Buttons & PSP_CTRL_TRIANGLE)) {
            sceKernelDelayThread(200000);
            break;
        }
        
	}

    free(paths);
    free(states);
}
