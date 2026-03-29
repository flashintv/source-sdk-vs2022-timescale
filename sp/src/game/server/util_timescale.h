#ifndef __UTIL_TIMESCALE__
#define __UTIL_TIMESCALE__
#ifdef _WIN32
#pragma once
#endif

// This might not work properly, if you're creating ConCommands or ConVars during runtime. 
// This will only work stable on server.
// 
// If you are either make them get created on startup or move the location of 
// TimeScale_Initialize from CServerGameDLL::PostInit to somewhere where you will know
// that your runtime ConCommands and ConVars will be created.
// This mostly applies to MetaMod/SourceMod.

void TimeScale_Initialize();
void TimeScale_Shutdown();

bool TimeScale_GetEnabled();
void TimeScale_SetEnabled( bool bEnable );

float TimeScale_GetScale();
void TimeScale_SetScale( float flScale = 1.0f );

#endif // __UTIL_TIMESCALE__