#include "cbase.h"
#include "util_timescale.h"
#include "convar.h"

ConVar* _sv_cheats;

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

	CUtlMap<const ConCommand*, ConCmdInfo> m_mapConCmds{ DefLessFunc( const ConCommand* ) };
	CUtlMap<const ConVar*, FnChangeCallback_t> m_mapConVars{ DefLessFunc( const ConVar* ) };

public:
	void Initialize();
	void Shutdown();

	bool IsInitialized() const { return m_bIsTimeScaleInitialized; }

	void SetEnabled( bool bEnable );
	bool IsEnabled() const { return m_bIsTimeScaleEnabled; }

	void SetScale( float flScale );
} g_TimeScale;

// Private functions
void Hook_FnCommandCallback( const CCommand& command )
{
	if ( !g_TimeScale.m_bAreCheatsEnabled )
	{
#ifdef _DEBUG
		Msg( "Cheat ConCommand '%s' disabled due to !g_TimeScale.m_bAreCheatsEnabled\n", command.Arg( 0 ) );
#endif
		return;
	}

	// Arg( 0 ) is the command name, at least to my knowledge...
	ConCommand* concmd = g_pCVar->FindCommand( command.Arg( 0 ) );
	if ( !concmd )
	{
		Warning( "TimeScale's ConCommand hook has encountered a non-existing command '%s'!\n", command.Arg( 0 ) );
		return;
	}
#ifdef _DEBUG
	Msg( "TimeScale's ConCommand hook has encountered a command '%s'!\n", command.Arg( 0 ) );
#endif

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
	bool bStoreAndIgnore = ( _sv_cheats == var && !g_TimeScale.m_bIgnoreCallback );
	if ( bStoreAndIgnore )
		g_TimeScale.m_bAreCheatsEnabled = _sv_cheats->GetInt();

	// Don't allow sv_cheats to actually be modified with TimeScale enabled. 
	// As doing 'sv_cheats 0' when TimeScale is enabled, will lead to host_timescale to be reset to 1.
	if ( bStoreAndIgnore || ( !g_TimeScale.m_bIgnoreCallback && !g_TimeScale.m_bAreCheatsEnabled ) )
	{
		g_TimeScale.m_bIgnoreCallback = true;
		var->SetValue( pOldValue );
		g_TimeScale.m_bIgnoreCallback = false;
#ifdef _DEBUG
		if ( !bStoreAndIgnore )
			Msg( "Cheat ConVar '%s' disabled due to !g_TimeScale.m_bAreCheatsEnabled\n", var->GetName() );
#endif
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
	for ( ConCommandBase* ccmdbase = g_pCVar->GetCommands(); ccmdbase != NULL; ccmdbase = ccmdbase->GetNext() )
	{
		if ( ccmdbase->IsCommand() )
		{
			ConCommand* ccmd = static_cast<ConCommand*>( ccmdbase );
			if ( ccmd->IsFlagSet( FCVAR_CHEAT ) )
			{
				m_mapConCmds.Insert( ccmd, {
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
			if ( convar->IsFlagSet( FCVAR_CHEAT ) )
			{
				m_mapConVars.Insert( convar, convar->m_fnChangeCallback );
				convar->m_fnChangeCallback = Hook_FnChangeCallback;
			}
			// + sv_cheats
			else if ( _sv_cheats == convar )
			{
				m_mapConVars.Insert( convar, convar->m_fnChangeCallback );
				convar->m_fnChangeCallback = Hook_FnChangeCallback;
			}
		}
	}

	// Get the sv_cheats as the server does not have an extern of it unlike client
	_sv_cheats = g_pCVar->FindVar( "sv_cheats" );

	// Disable the notify flag for sv_cheats
	if ( _sv_cheats && _sv_cheats->m_pParent )
		_sv_cheats->m_pParent->m_nFlags &= FCVAR_NOTIFY;

	m_bIsTimeScaleInitialized = true;
}

void TimeScale::Shutdown()
{
	// Enable back the notify flag for sv_cheats
	if ( _sv_cheats && _sv_cheats->m_pParent )
		_sv_cheats->m_pParent->m_nFlags |= FCVAR_NOTIFY;

	m_bIsTimeScaleInitialized = false;
}

void TimeScale::SetEnabled( bool bEnable ) 
{ 
	m_bIsTimeScaleEnabled = bEnable; 
	m_bAreCheatsEnabled = _sv_cheats->GetBool();

	m_bIgnoreCallback = true;
	if ( bEnable && !m_bAreCheatsEnabled )
		_sv_cheats->SetValue( true );
	else if ( !bEnable )
		_sv_cheats->SetValue( false );
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
void TimeScale_Initialize() { g_TimeScale.Initialize(); }
void TimeScale_Shutdown() { g_TimeScale.Shutdown(); }

bool TimeScale_GetEnabled() { return g_TimeScale.IsEnabled(); }
void TimeScale_SetEnabled( bool bEnable ) { g_TimeScale.SetEnabled( bEnable ); }

float TimeScale_GetScale()
{
	static ConVarRef host_timescale( "host_timescale" );
	return host_timescale.GetFloat();
}
void TimeScale_SetScale( float flScale ) { g_TimeScale.SetScale( flScale ); }

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
#endif