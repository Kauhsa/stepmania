#ifndef PREFSMANAGER_H
#define PREFSMANAGER_H
/*
-----------------------------------------------------------------------------
 Class: PrefsManager

 Desc: Holds user-chosen preferences that are saved between sessions.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Chris Danford
-----------------------------------------------------------------------------
*/


class PrefsManager
{
public:
	PrefsManager();
	~PrefsManager();

	// GameOptions (ARE saved between sessions)
	bool			m_bWindowed;
	int				m_iDisplayWidth;
	int				m_iDisplayHeight;
	int				m_iDisplayColorDepth;
	int				m_iTextureColorDepth;
	int				m_iMaxTextureResolution;
	int				m_iRefreshRate;
	bool			m_bShowStats;
	enum { BGMODE_OFF, BGMODE_ANIMATIONS, BGMODE_MOVIEVIS, BGMODE_RANDOMMOVIES } m_BackgroundMode;
	float			m_fBGBrightness;
	int 			m_iMovieDecodeMS;
	bool			m_bUseBGIfNoBanner;
	bool			m_bHiddenSongs;
	bool			m_bVsync;
	int				m_iUnloadTextureDelaySeconds;

	bool			m_bIgnoreJoyAxes;	
	bool			m_bOnlyDedicatedMenuButtons;
	bool			m_bMenuTimer;
	bool			m_bShowDanger;
	int				m_iNumArcadeStages;
	bool			m_bEventMode;
	float			m_fJudgeWindowScale;
	float			m_fLifeDifficultyScale;
	float			m_fJudgeWindowMarvelousSeconds;
	float			m_fJudgeWindowPerfectSeconds;
	float			m_fJudgeWindowGreatSeconds;
	float			m_fJudgeWindowGoodSeconds;
	float			m_fJudgeWindowBooSeconds;
	bool			m_bAutoPlay;
	bool			m_bDelayedEscape;
	bool			m_bInstructions, m_bShowDontDie, m_bShowSelectGroup;
	bool			m_bArcadeOptionsNavigation;
	bool			m_bCoinOpMode;
	bool			m_bMusicWheelUsesSections;
	int				m_iMusicWheelSwitchSpeed;
	bool			m_bChangeBannersWhenFast;
	bool			m_bEasterEggs;
	bool			m_bMarvelousTiming;
	enum { COIN_HOME, COIN_PAY, COIN_FREE, NUM_COIN_MODES } m_CoinMode;
	int				m_iCoinsPerCredit;
	bool			m_bJointPremium;


	/* 0 = no; 1 = yes; -1 = auto (do whatever is appropriate for the arch). */
	int				m_iBoostAppPriority;

	CStringArray m_asAdditionalSongFolders;
	CString m_DWIPath;

	CString			m_sSoundDrivers;
	float			m_fSoundVolume;

	void ReadGlobalPrefsFromDisk( bool bSwitchToLastPlayedGame );
	void SaveGlobalPrefsToDisk();


	void ReadGamePrefsFromDisk();
	void SaveGamePrefsToDisk();
};


extern PrefsManager*	PREFSMAN;	// global and accessable from anywhere in our program

#endif
