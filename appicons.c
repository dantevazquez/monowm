#define _GNU_SOURCE
#include <string.h>
#include <strings.h>
#include "appicons.h"
#include "config.h"

const char* get_default_icon(){
    return DEFAULT_ICON_STR;
}

const char* get_icon_by_name(const char *name){

    if(!name) return DEFAULT_ICON_STR;

    for(int i = 0; APP_ICONS[i].name != NULL; i++){
        if(strcasestr(name, APP_ICONS[i].name) != NULL){
            return APP_ICONS[i].icon;
        }
    }
    return DEFAULT_ICON_STR;
}



