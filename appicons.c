#define _GNU_SOURCE
#include <string.h>
#include <strings.h>
#include "appicons.h"
#include "config.h"

const char* get_default_icon(){
    return config.default_icon_str;
}

const char* get_icon_by_name(const char *name){

    if(!name) return config.default_icon_str;

    for(int i = 0; i < config.app_icon_count; i++){
        if(strcasestr(name, config.app_icons[i].name) != NULL){
            return config.app_icons[i].icon;
        }
    }
    return config.default_icon_str;
}




