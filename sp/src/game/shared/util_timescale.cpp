#include "cbase.h"
#include "util_timescale.h"
#include "convar.h"

ConVar net_sv_cheats( "net_sv_cheats", "0", FCVAR_DEVELOPMENTONLY|FCVAR_HIDDEN|FCVAR_REPLICATED );

#ifdef GAME_DLL
ConVar* g_sv_cheats = NULL;
ConVar* g_mat_wireframe = NULL;

class TimeScale
{
private:
	friend void Hook_FnCommandCallback( const CCommand& command );
	friend void Hook_FnChangeCallback( IConVar* var, const char* pOldValue, float flOldValue );

	struct ConCmdInfo
	{
		bool m_bUsingNewCommandCallback;
		bool m_bUsingCommandCallbackInterface;
		void* m_pFuncOrInterface;
	};

	bool m_bIsTimeScaleInitialized{ false };
	bool m_bIsTimeScaleEnabled{ false };
	bool m_bAreCheatsEnabled{ false };
	bool m_bIgnoreCallback{ false };

	CUtlMap<ConCommand*, ConCmdInfo> m_mapConCmds{ DefLessFunc( ConCommand* ) };
	CUtlMap<ConVar*, FnChangeCallback_t> m_mapConVars{ DefLessFunc( ConVar* ) };

public:
	void Initialize();
	void Shutdown();

	bool AreCheatsAllowed() const { return m_bAreCheatsEnabled; }

	bool IsInitialized() const { return m_bIsTimeScaleInitialized; }

	void SetEnabled( bool bEnable );
	bool IsEnabled() const { return m_bIsTimeScaleEnabled; }

	void SetScale( float flScale );
} g_TimeScale;

// Private functions
void Hook_FnCommandCallback( const CCommand& command )
{
	// Arg( 0 ) is the command name, at least to my knowledge...
	ConCommand* concmd = g_pCVar->FindCommand( command.Arg( 0 ) );

#ifdef MULTIPLAYER_MODE // You can use cheats in singleplayer
	if ( g_TimeScale.m_bIsTimeScaleEnabled )
	{
		if ( !g_TimeScale.m_bAreCheatsEnabled )
		{
			Msg( "Can't use cheat command %s in multiplayer, unless the server has sv_cheats set to 1.\n", concmd->GetName() );
			return;
		}
	}
#endif

	if ( !concmd )
	{
		Warning( "TimeScale's ConCommand hook has encountered a non-existing command '%s'!\n", command.Arg( 0 ) );
		return;
	}

	unsigned short idx = g_TimeScale.m_mapConCmds.Find( concmd );
	if ( idx == g_TimeScale.m_mapConCmds.InvalidIndex() )
	{
		Warning( "TimeScale's ConCommand hook has encountered a command '%s' that doesn't exist in our mapping! How are we hooked?!\n", command.Arg( 0 ) );
		return;
	}

	const TimeScale::ConCmdInfo& info = g_TimeScale.m_mapConCmds[ idx ];
	if ( info.m_bUsingNewCommandCallback )
	{
		if ( info.m_pFuncOrInterface )
		{
			(*static_cast<FnCommandCallback_t>( info.m_pFuncOrInterface ))(command);
			return;
		}
	}
	else if ( info.m_bUsingCommandCallbackInterface )
	{
		if ( info.m_pFuncOrInterface )
		{
			static_cast<ICommandCallback*>( info.m_pFuncOrInterface )->CommandCallback( command );
			return;
		}
	}
	else
	{
		if ( info.m_pFuncOrInterface )
		{
			(*static_cast<FnCommandCallbackVoid_t>( info.m_pFuncOrInterface ))();
			return;
		}
	}
}

void Hook_FnChangeCallback( IConVar* var, const char* pOldValue, float flOldValue )
{
	// We store the set value sv_cheats of if it wasn't enabled by TimeScale!
	bool bStoreAndIgnore = (g_sv_cheats == var && !g_TimeScale.m_bIgnoreCallback);
	if ( bStoreAndIgnore ) {
		g_TimeScale.m_bAreCheatsEnabled = g_sv_cheats->GetInt();
		net_sv_cheats.SetValue( g_TimeScale.m_bAreCheatsEnabled );
	}

	if ( g_TimeScale.m_bIsTimeScaleEnabled )
	{
#ifdef MULTIPLAYER_MODE // You can use cheats in singleplayer
		// Don't allow sv_cheats to actually be modified with TimeScale enabled. 
		// As doing 'sv_cheats 0' when TimeScale is enabled, will lead to host_timescale to be reset to 1.
		if ( bStoreAndIgnore || ( !g_TimeScale.m_bIgnoreCallback && !g_TimeScale.m_bAreCheatsEnabled ) )
		{
			g_TimeScale.m_bIgnoreCallback = true;
			var->SetValue( pOldValue );
			g_TimeScale.m_bIgnoreCallback = false;

			if ( !bStoreAndIgnore )
				Msg( "Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1.\n", var->GetName() );
			return;
		}
#else
		if ( bStoreAndIgnore )
		{
			g_TimeScale.m_bIgnoreCallback = true;
			var->SetValue( pOldValue );
			g_TimeScale.m_bIgnoreCallback = false;
			return;
		}
#endif
	}

	// Besides mat_wireframe as it's a weird convar, that uses an inline function to check for its value.
	if ( var == g_mat_wireframe && !g_TimeScale.m_bAreCheatsEnabled && !g_TimeScale.m_bIgnoreCallback )
	{
		g_TimeScale.m_bIgnoreCallback = true;
		var->SetValue( false );
		g_TimeScale.m_bIgnoreCallback = false;
		return;
	}

	unsigned short idx = g_TimeScale.m_mapConVars.Find( (ConVar*)var );
	if ( idx == g_TimeScale.m_mapConVars.InvalidIndex() )
	{
		Warning( "TimeScale's ConVar hook has encountered a console variable '%s' that doesn't exist in our mapping! How are we hooked?!\n", var->GetName() );
		return;
	}

	FnChangeCallback_t fnCallback = g_TimeScale.m_mapConVars[idx];
	if ( fnCallback )
		fnCallback( var, pOldValue, flOldValue );
}

void TimeScale::Initialize()
{
	g_sv_cheats = g_pCVar->FindVar( "sv_cheats" );
	g_mat_wireframe = g_pCVar->FindVar( "mat_wireframe" );

#ifdef MULTIPLAYER_MODE
	for ( ConCommandBase* ccmdbase = g_pCVar->GetCommands(); ccmdbase != NULL; ccmdbase = ccmdbase->GetNext() )
	{
		if ( ccmdbase->IsCommand() )
		{
			ConCommand* ccmd = static_cast<ConCommand*>( ccmdbase );
			if ( ccmd->IsFlagSet( FCVAR_CHEAT ) )
			{
				m_mapConCmds.InsertOrReplace( ccmd, {
					ccmd->m_bUsingNewCommandCallback,
					ccmd->m_bUsingCommandCallbackInterface,
					ccmd->m_fnCommandCallback
				} );

				ccmd->m_bUsingNewCommandCallback = true;
				ccmd->m_bUsingCommandCallbackInterface = false;
				ccmd->m_fnCommandCallback = Hook_FnCommandCallback;
			}
		}
		else
		{
			ConVar* convar = static_cast<ConVar*>( ccmdbase );
			if ( convar->IsFlagSet( FCVAR_CHEAT ) || /* + sv_cheats */ g_sv_cheats == convar )
			{
				m_mapConVars.InsertOrReplace( convar, convar->m_fnChangeCallback );
				convar->m_fnChangeCallback = Hook_FnChangeCallback;
			}
		}
	}
#else
	if ( g_sv_cheats )
	{
		m_mapConVars.InsertOrReplace( g_sv_cheats, g_sv_cheats->m_fnChangeCallback );
		g_sv_cheats->m_fnChangeCallback = Hook_FnChangeCallback;
	}
	if ( g_mat_wireframe )
	{
		m_mapConVars.InsertOrReplace( g_mat_wireframe, g_mat_wireframe->m_fnChangeCallback );
		g_mat_wireframe->m_fnChangeCallback = Hook_FnChangeCallback;
	}
#endif

	// Disable the notify flag for sv_cheats
	if ( g_sv_cheats && g_sv_cheats->m_pParent )
		g_sv_cheats->m_pParent->m_nFlags &= ~FCVAR_NOTIFY;

	m_bIsTimeScaleInitialized = true;
}

void TimeScale::Shutdown()
{
	// Enable back the notify flag for sv_cheats
	if ( g_sv_cheats && g_sv_cheats->m_pParent )
		g_sv_cheats->m_pParent->m_nFlags |= FCVAR_NOTIFY;

	FOR_EACH_MAP( m_mapConCmds, i )
	{
		ConCommand* pConCmd = m_mapConCmds.Key( i );
		ConCmdInfo& cmdInfo = m_mapConCmds.Element( i );

		pConCmd->m_bUsingNewCommandCallback = cmdInfo.m_bUsingCommandCallbackInterface;
		pConCmd->m_bUsingCommandCallbackInterface = cmdInfo.m_bUsingCommandCallbackInterface;

		// It doesn't matter what we set it to they are all pointers...
		pConCmd->m_fnCommandCallback = (FnCommandCallback_t)cmdInfo.m_pFuncOrInterface;
	}
	FOR_EACH_MAP( m_mapConVars, i )
	{
		m_mapConVars.Key( i )->m_fnChangeCallback = m_mapConVars.Element( i );
	}

	m_bIsTimeScaleInitialized = false;
}

void TimeScale::SetEnabled( bool bEnable ) 
{ 
	m_bIsTimeScaleEnabled = bEnable; 
	m_bAreCheatsEnabled = g_sv_cheats->GetBool();

	m_bIgnoreCallback = true;

	if ( bEnable && !m_bAreCheatsEnabled )
		g_sv_cheats->SetValue( true );
	else if ( !bEnable )
		g_sv_cheats->SetValue( false );

	m_bIgnoreCallback = false;
}

void TimeScale::SetScale( float flScale )
{
	m_bIgnoreCallback = true;

	static ConVarRef host_timescale( "host_timescale" );
	host_timescale.SetValue( flScale );

	m_bIgnoreCallback = false;
}

// Public functions
void TimeScale_Initialize() 
{ 
	if ( !g_TimeScale.IsInitialized() ) 
		g_TimeScale.Initialize();
}
void TimeScale_Shutdown() 
{ 
	if ( g_TimeScale.IsInitialized() ) 
		g_TimeScale.Shutdown();
}

bool TimeScale_AreCheatsAllowed()
{
	static ConVarRef ref_sv_cheats( "sv_cheats" );
	return g_TimeScale.IsInitialized() ? g_TimeScale.AreCheatsAllowed() : ref_sv_cheats.GetBool();
}

bool TimeScale_GetEnabled() 
{ 
	return g_TimeScale.IsInitialized() ? g_TimeScale.IsEnabled() : false; 
}
void TimeScale_SetEnabled( bool bEnable ) 
{ 
	if ( g_TimeScale.IsInitialized() ) 
		g_TimeScale.SetEnabled( bEnable ); 
}

float TimeScale_GetScale()
{
	static ConVarRef host_timescale( "host_timescale" );
	return host_timescale.GetFloat();
}
void TimeScale_SetScale( float flScale ) 
{
	if ( g_TimeScale.IsInitialized() )
		g_TimeScale.SetScale( flScale ); 
}

// Debug functions
#ifdef _DEBUG
CON_COMMAND( timescale_setenable, "Enable TimeScale to test it" )
{
	if ( args.ArgC() < 1 ) return;

	bool status = Q_atoi( args.Arg( 1 ) );
	Msg( "%s TimeScale!\n", status ? "Enabling" : "Disabling" );
	TimeScale_SetEnabled( status );
}
CON_COMMAND( timescale_setscale, "Disable TimeScale to test it" )
{
	if ( args.ArgC() < 1 ) return;

	float scale = Q_atof( args.Arg( 1 ) );
	Msg( "Setting host_timescale to %f!\n", scale );
	TimeScale_SetScale( scale );
}
#endif // _DEBUG

#endif // GAME_DLL