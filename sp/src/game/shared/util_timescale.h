#ifndef __UTIL_TIMESCALE__
#define __UTIL_TIMESCALE__
#ifdef _WIN32
#pragma once
#endif

// This might not work properly, if you're creating ConCommands or ConVars during runtime.
// If you are doing either - make them get created on startup or move the location of 
// TimeScale_Initialize from CServerGameDLL::PostInit to somewhere where you will know
// that your runtime ConCommands and ConVars will be created by the time it executes.
// This applies to MetaMod/SourceMod first and foremost. 

// Currently I only know that this code works on an SP mod, untested on MP.
// Enable this if it's not a singleplayer mod.
//#define MULTIPLAYER_MODE

#ifdef GAME_DLL
void TimeScale_Initialize();
void TimeScale_Shutdown();

bool TimeScale_AreCheatsAllowed();

bool TimeScale_GetEnabled();
void TimeScale_SetEnabled( bool bEnable );

float TimeScale_GetScale();
void TimeScale_SetScale( float flScale = 1.0f );
#endif

extern ConVar net_sv_cheats;

#endif // __UTIL_TIMESCALE__