#pragma once

// When IntelliSense parses files outside the active env,
// allow both role branches to be visible.
#ifdef __INTELLISENSE__
  #ifndef ROLE_CONTROLLER
    #define ROLE_CONTROLLER
  #endif
  #ifndef ROLE_RECEIVER
    #define ROLE_RECEIVER
  #endif
#endif
