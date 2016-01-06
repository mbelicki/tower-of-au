#pragma once

#if not (defined(IOS) || defined(VIM_YCM))
    #include "version_gen.h"
#endif

#if not defined(VERSION)
    #define VERSION "missing version info"
#endif

#if not defined(PROJECT_NAME)
    #define PROJECT_NAME "missing-project-name"
#endif

#if not defined(APP_NAME)
    #define APP_NAME "missing app name"
#endif
