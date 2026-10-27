/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// cl_scrn.c -- master for refresh, status bar, console, chat, notify, etc

#include "client.h"
#include "cl_uiapi.h"
#include "cl_cgameapi.h"

extern console_t con;
qboolean	scr_initialized;		// ready to draw
int			g_clientRankedEnabled = 1;

cvar_t		*cl_timegraph;
cvar_t		*cl_debuggraph;
cvar_t		*cl_graphheight;
cvar_t		*cl_graphscale;
cvar_t		*cl_graphshift;

cvar_t		*cg_drawRPGHUD;
cvar_t		*cg_rpg_style;
cvar_t		*cg_rpg_pos;
cvar_t		*cg_rpg_x;
cvar_t		*cg_rpg_y;
cvar_t		*cg_rpg_level;
cvar_t		*cg_rpg_xp;
cvar_t		*cg_rpg_xp_max;
cvar_t		*cg_rpg_fr;
cvar_t		*cg_rpg_avatar;
cvar_t		*cg_rpg_name;
cvar_t		*cg_rpg_rank;
cvar_t		*cg_drawLeaderboard;
cvar_t		*cg_drawStats;
cvar_t		*cg_drawBounty;
cvar_t		*cg_drawShop;
cvar_t		*cg_drawQuest;
cvar_t		*cg_drawInventory;
cvar_t		*cg_drawAch;
cvar_t		*cg_drawTopCredits;
cvar_t		*cg_drawTopPotato;
cvar_t		*cg_drawAdv;
static void SCR_ColUITest_f( void );
static void SCR_UIScreenTest_f( void );
static void SCR_RPGUITest_f( void );
static void SCR_RPGXPTest_f( void );
int g_rpgXpPerLevel = 1000;
int g_rpgRankCount = 0;
rpgRankEntry_t g_rpgRanks[8];
rpgPlayerStats_t g_rpgStats = {qfalse, 0, 1, 0, 1000, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Padawan", "", "", "", "", "Lightsaber"};


rpgToastNotif_t  g_rpgToast   = {qfalse, qfalse, 0, 0, 0, 0, 0, "", 0};

rpgInspectCard_t g_rpgInspect = {qfalse, 1, 1000, "Padawan", "", 0};
rpgBountyOverlay_t g_rpgBounty = {qfalse, qfalse, 0, {}};
int                   g_hotPotatoHolder  = -1;
rpgShopOverlay_t      g_rpgShop          = {qfalse, 0, 0, {}};
rpgSettingsOverlay_t  g_rpgSettings      = {qfalse};
rpgQuestOverlay_t      g_rpgQuest         = {qfalse, 0, {}};
rpgInventoryOverlay_t  g_rpgInventory     = {qfalse, 0, {}, 0};
rpgAchOverlay_t       g_rpgAch           = {qfalse, 0, {}};
rpgTopCreditsOverlay_t g_rpgTopCredits   = {qfalse, 0, {}};
rpgTopPotatoOverlay_t g_rpgTopPotato     = {qfalse, 0, {}};
rpgAdventureOverlay_t g_rpgAdv           = {qfalse, "", "", "", "", ""};
rpgPartyOverlay_t     g_rpgParty         = {qfalse, "", 0, 0, {}};
rpgMenuOverlay_t        g_rpgMenu        = {qfalse, 0};
rpgPartyStudioOverlay_t g_rpgPartyStudio = {qfalse, 0};
rpgAdminOverlay_t       g_rpgAdmin       = {qfalse, -1, 0};




/*
================
SCR_DrawNamedPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawNamedPic( float x, float y, float width, float height, const char *picname ) {
	qhandle_t	hShader;

	assert( width != 0 );

	hShader = re->RegisterShader( picname );
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}


// UI animation state: while an overlay is wrapped in SCR_UIAnimBegin/End every rect, pic and string is
// faded by s_uiAlpha and moved by s_uiDY, so whole windows can fade/slide in without touching their code.
static float s_uiAlpha = 1.0f;
static float s_uiDY = 0.0f;
static qboolean s_uiAnimating = qfalse;
static qboolean s_uiDrew = qfalse;

/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
	if ( s_uiAnimating ) {
		vec4_t c = { 1.0f, 1.0f, 1.0f, 1.0f };
		if ( color ) { c[0] = color[0]; c[1] = color[1]; c[2] = color[2]; c[3] = color[3]; }
		c[3] *= s_uiAlpha;
		s_uiDrew = qtrue;
		re->SetColor( c );
		re->DrawStretchPic( x, y + s_uiDY, width, height, 0, 0, 0, 0, cls.whiteShader );
		re->SetColor( NULL );
		return;
	}

	re->SetColor( color );

	re->DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cls.whiteShader );

	re->SetColor( NULL );
}


void SCR_DrawPartyOverlay( void );

/*
================
SCR_DrawPic

Coordinates are 640*480 virtual values
=================
*/
void SCR_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	if ( s_uiAnimating ) {
		vec4_t c = { 1.0f, 1.0f, 1.0f, s_uiAlpha };
		re->SetColor( c );
		re->DrawStretchPic( x, y + s_uiDY, width, height, 0, 0, 1, 1, hShader );
		re->SetColor( NULL );
		return;
	}
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}



float SCR_GetCharWidthFactor( int ch ) {
	return 0.50f;
}

// ---------------------------------------------------------------------------
// UI text: proportional MBII/JKA font instead of the fixed-width char sheet.
// Falls back to the old char sheet when no font can be registered or the text has emoji glyphs.
// ---------------------------------------------------------------------------
static int s_uiFont = -1;	// -1 = not tried yet, 0 = none available

static int SCR_UIFont( void ) {
	if ( s_uiFont < 0 ) {
		s_uiFont = 0;
		if ( re && re->RegisterFont ) {
			static const char *names[] = { "ergoec", "arialnb", "ocr_a", NULL };
			for ( int i = 0; names[i] && s_uiFont <= 0; i++ ) {
				s_uiFont = re->RegisterFont( names[i] );
				if ( s_uiFont < 0 ) s_uiFont = 0;
			}
		}
	}
	return s_uiFont;
}

static qboolean SCR_HasEmojiGlyph( const char *str ) {
	for ( const unsigned char *p = (const unsigned char *)str; *p; p++ ) {
		if ( *p >= 0x80 && *p <= 0x8F ) return qtrue;
	}
	return qfalse;
}

// Font scale that gives roughly the same glyph height as the old char cells for a given charSize
static float SCR_UIFontScale( int font, float charSize ) {
	float renderH = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
	int h = re->Font_HeightPixels( font, 1.0f );
	if ( h < 1 ) h = 1;
	return renderH * 1.15f / (float)h;
}

float SCR_GetStringWidth( const char *str, float charSize ) {
	int font = SCR_UIFont();
	if ( font && !SCR_HasEmojiGlyph( str ) ) {
		return (float)re->Font_StrLenPixels( str, font, SCR_UIFontScale( font, charSize ) );
	}
	float renderH = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
	return (float)Q_PrintStrlen( str ) * renderH * 0.50f;
}

/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size with clean proportional width ratio
*/
static void SCR_DrawChar( int x, int y, float size, int ch ) {
	int row, col;
	float frow, fcol;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch >= 0x80 && ch <= 0x8F ) {
		static qhandle_t s_hEmojiShaders[16] = { 0 };
		static const char *s_emojiShaderNames[16] = {
			"gfx/rpg_hud/emoji_fire",
			"gfx/rpg_hud/emoji_potato",
			"gfx/rpg_hud/emoji_swords",
			"gfx/rpg_hud/emoji_crown",
			"gfx/rpg_hud/emoji_trophy",
			"gfx/rpg_hud/emoji_skull",
			"gfx/rpg_hud/emoji_100",
			"gfx/rpg_hud/emoji_heart",
			"gfx/rpg_hud/emoji_star",
			"gfx/rpg_hud/emoji_zap",
			"gfx/rpg_hud/emoji_flex",
			"gfx/rpg_hud/emoji_gg",
			"gfx/rpg_hud/emoji_thumbsup",
			"gfx/rpg_hud/emoji_target",
			"gfx/rpg_hud/emoji_rocket",
			"gfx/rpg_hud/emoji_poop"
		};
		int idx = ch - 0x80;
		if ( s_hEmojiShaders[idx] <= 0 && re && re->RegisterShader ) {
			s_hEmojiShaders[idx] = re->RegisterShader( s_emojiShaderNames[idx] );
		}
		if ( s_hEmojiShaders[idx] > 0 ) {
			re->DrawStretchPic( x, y, size * 1.5f, size * 1.5f, 0, 0, 1, 1, s_hEmojiShaders[idx] );
			return;
		}

	}

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -size ) {
		return;
	}

	ax = x;
	ay = y;
	aw = size * 0.50f;
	ah = size;

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625f;
	fcol = col*0.0625f;
	float uSize = 0.03125f;
	float vSize = 0.0625f;

	re->DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow,
					   fcol + uSize, frow + vSize,
					   cls.charSetShader );
}

/*
** SCR_DrawSmallChar
** small chars are drawn at native screen resolution
*/
void SCR_DrawSmallChar( int x, int y, int ch ) {
	int row, col;
	float frow, fcol;
	float size;

	ch &= 255;

	if ( ch >= 0x80 && ch <= 0x8F ) {
		static qhandle_t s_hEmojiShadersSmall[16] = { 0 };
		static const char *s_emojiShaderNamesSmall[16] = {
			"gfx/rpg_hud/emoji_fire",
			"gfx/rpg_hud/emoji_potato",
			"gfx/rpg_hud/emoji_swords",
			"gfx/rpg_hud/emoji_crown",
			"gfx/rpg_hud/emoji_trophy",
			"gfx/rpg_hud/emoji_skull",
			"gfx/rpg_hud/emoji_100",
			"gfx/rpg_hud/emoji_heart",
			"gfx/rpg_hud/emoji_star",
			"gfx/rpg_hud/emoji_zap",
			"gfx/rpg_hud/emoji_flex",
			"gfx/rpg_hud/emoji_gg",
			"gfx/rpg_hud/emoji_thumbsup",
			"gfx/rpg_hud/emoji_target",
			"gfx/rpg_hud/emoji_rocket",
			"gfx/rpg_hud/emoji_poop"
		};
		int idx = ch - 0x80;
		if ( s_hEmojiShadersSmall[idx] <= 0 && re && re->RegisterShader ) {
			s_hEmojiShadersSmall[idx] = re->RegisterShader( s_emojiShaderNamesSmall[idx] );
		}
		if ( s_hEmojiShadersSmall[idx] > 0 ) {
			re->DrawStretchPic( x * con.xadjust, y * con.yadjust,
								SMALLCHAR_WIDTH * 1.6f * con.xadjust, SMALLCHAR_HEIGHT * 1.6f * con.yadjust,
								0, 0, 1, 1, s_hEmojiShadersSmall[idx] );
			return;
		}

	}

	if ( ch == ' ' ) {
		return;
	}

	if ( y < -SMALLCHAR_HEIGHT ) {
		return;
	}

	row = ch>>4;
	col = ch&15;

	float size2;

	frow = row*0.0625f;
	fcol = col*0.0625f;

	size = 0.03125f;
	size2 = 0.0625f;

	re->DrawStretchPic( x * con.xadjust, y * con.yadjust,
						SMALLCHAR_WIDTH * con.xadjust, SMALLCHAR_HEIGHT * con.yadjust,
					   fcol, frow,
					   fcol + size, frow + size2,
					   cls.charSetShader );
}



/*
==================
SCR_DrawBigString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawStringExt( int x, int y, float size, const char *string, float *setColor, qboolean forceColor, qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	color[0] = color[1] = color[2] = 0;
	color[3] = setColor[3];
	re->SetColor( color );
	s = string;
	xx = x;
	while ( *s ) {
		if ( !noColorEscape && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx+2, y+2, size, *s );
		xx += size * SCR_GetCharWidthFactor( *s );
		s++;
	}


	// draw the colored text
	s = string;
	xx = x;
	re->SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor[3];
				re->SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawChar( xx, y, size, *s );
		xx += size * SCR_GetCharWidthFactor( *s );
		s++;
	}
	re->SetColor( NULL );
}


void SCR_DrawBigString( int x, int y, const char *s, float alpha, qboolean noColorEscape ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qfalse, noColorEscape );
}

void SCR_DrawBigStringColor( int x, int y, const char *s, vec4_t color, qboolean noColorEscape ) {
	SCR_DrawStringExt( x, y, BIGCHAR_WIDTH, s, color, qtrue, noColorEscape );
}


/*
==================
SCR_DrawSmallString[Color]

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void SCR_DrawSmallStringExt( int x, int y, const char *string, float *setColor, qboolean forceColor, qboolean noColorEscape ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the colored text
	s = string;
	xx = x;
	re->SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				Com_Memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor[3];
				re->SetColor( color );
			}
			if ( !noColorEscape ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawSmallChar( xx, y, *s );
		xx += SMALLCHAR_WIDTH;
		s++;
	}
	re->SetColor( NULL );
}



/*
** SCR_Strlen -- skips color escape codes
*/
static int SCR_Strlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
** SCR_GetBigStringWidth
*/
int	SCR_GetBigStringWidth( const char *str ) {
	return SCR_Strlen( str ) * BIGCHAR_WIDTH;
}


//===============================================================================

/*
=================
SCR_DrawDemoRecording
=================
*/
void SCR_DrawDemoRecording( void ) {
	char	string[1024];
	int		pos;

	if ( !clc.demorecording ) {
		return;
	}
	if ( clc.spDemoRecording ) {
		return;
	}
	if (!cl_drawRecording->integer) {
		return;
	}
	pos = FS_FTell( clc.demofile );
	Com_sprintf( string, sizeof(string), "RECORDING %s: %ik", clc.demoName, pos / 1024 );

	SCR_DrawStringExt( 320 - strlen( string ) * 4, 20, 8, string, g_color_table[7], qtrue, qfalse );
}


/*
===============================================================================

DEBUG GRAPH

===============================================================================
*/

typedef struct graphsamp_s {
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph (float value, int color)
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
void SCR_DrawDebugGraph (void)
{
	int		a, x, y, w, i, h;
	float	v;

	//
	// draw the graph
	//
	w = 640;
	x = 0;
	y = 480;
	re->SetColor( g_color_table[0] );
	re->DrawStretchPic(x, y - cl_graphheight->integer,
		w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re->SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		v = v * cl_graphscale->integer + cl_graphshift->integer;
		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re->DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}

//=============================================================================

/*
==================
SCR_RPGHUDStyle_f

Console command: rpg_hud_style <classic|bottom|0|1>
==================
*/
static void SCR_RPGHUDStyle_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "^3Usage: ^7rpg_hud_style <holo | edge | 0 | 1>\n" );
		return;
	}
	const char *style = Cmd_Argv( 1 );
	if ( Cmd_Argc() >= 3 && !Q_stricmp( style, "style" ) ) {
		style = Cmd_Argv( 2 );
	}
	if ( !Q_stricmp( style, "edge" ) || !Q_stricmp( style, "1" ) ) {
		Cvar_Set( "cg_rpg_style", "1" );
		Com_Printf( "^2RPG HUD style set to EDGE CARD\n" );
	} else {
		Cvar_Set( "cg_rpg_style", "0" );
		Com_Printf( "^2RPG HUD style set to HOLO CARD\n" );
	}
}

/*
==================
SCR_RPGHUDPos_f

Console command: rpg_hud_pos <left|right|bottomright|bottomleft|bottomcenter>
==================
*/
static void SCR_RPGHUDPos_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "^3Usage: ^7rpg_hud_pos <left | right | bottomright | bottomleft | bottomcenter>\n" );
		return;
	}
	const char *pos = Cmd_Argv( 1 );
	if ( !Q_stricmp( pos, "left" ) || !Q_stricmp( pos, "topleft" ) ) {
		Cvar_Set( "cg_rpg_pos", "left" );
		Cvar_Set( "cg_rpg_x", "14" );
		Cvar_Set( "cg_rpg_y", "14" );
		Com_Printf( "^2RPG HUD set to TOP LEFT (14, 14)\n" );
	} else if ( !Q_stricmp( pos, "right" ) || !Q_stricmp( pos, "topright" ) ) {
		Cvar_Set( "cg_rpg_pos", "right" );
		Cvar_Set( "cg_rpg_x", "468" );
		Cvar_Set( "cg_rpg_y", "14" );
		Com_Printf( "^2RPG HUD set to TOP RIGHT (468, 14)\n" );
	} else if ( !Q_stricmp( pos, "bottomright" ) ) {
		Cvar_Set( "cg_rpg_pos", "bottomright" );
		Cvar_Set( "cg_rpg_x", "468" );
		Cvar_Set( "cg_rpg_y", "345" );
		Com_Printf( "^2RPG HUD set to BOTTOM RIGHT (468, 345)\n" );
	} else if ( !Q_stricmp( pos, "bottomleft" ) ) {
		Cvar_Set( "cg_rpg_pos", "bottomleft" );
		Cvar_Set( "cg_rpg_x", "14" );
		Cvar_Set( "cg_rpg_y", "345" );
		Com_Printf( "^2RPG HUD set to BOTTOM LEFT (14, 345)\n" );
	} else if ( !Q_stricmp( pos, "bottomcenter" ) || !Q_stricmp( pos, "center" ) ) {
		Cvar_Set( "cg_rpg_pos", "bottomcenter" );
		Cvar_Set( "cg_rpg_x", "210" );
		Cvar_Set( "cg_rpg_y", "428" );
		Com_Printf( "^2RPG HUD set to BOTTOM CENTER (210, 428)\n" );
	} else {
		Com_Printf( "^1Unknown position '%s'. Use ^3left^1, ^3right^1, ^3bottomright^1, ^3bottomleft^1, or ^3bottomcenter^1.\n", pos );
	}
}

static void SCR_HUDSettings_f( void ) {
	g_rpgSettings.active = (qboolean)(!g_rpgSettings.active);
	Com_Printf( "^2RPG HUD settings studio %s\n", g_rpgSettings.active ? "OPENED" : "CLOSED" );
}

static void SCR_RPGMenu_f( void ) {
	g_rpgMenu.active = (qboolean)(!g_rpgMenu.active);
	Com_Printf( "^2Master RPG Hub Menu %s\n", g_rpgMenu.active ? "OPENED" : "CLOSED" );
}

static void SCR_PartyMenu_f( void ) {
	g_rpgPartyStudio.active = (qboolean)(!g_rpgPartyStudio.active);
	Com_Printf( "^2Party Management Studio %s\n", g_rpgPartyStudio.active ? "OPENED" : "CLOSED" );
}

static void SCR_AdminMenu_f( void ) {
	g_rpgAdmin.active = (qboolean)(!g_rpgAdmin.active);
	Com_Printf( "^2Admin Control Panel %s\n", g_rpgAdmin.active ? "OPENED" : "CLOSED" );
}

void SCR_Games_f( void );
void SCR_Blackjack_f( void );
void SCR_Pazaak_f( void );

void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

	cg_drawRPGHUD = Cvar_Get ("cg_drawRPGHUD", "1", CVAR_ARCHIVE);
	cg_rpg_style = Cvar_Get ("cg_rpg_style", "0", CVAR_ARCHIVE);
	cg_rpg_pos = Cvar_Get ("cg_rpg_pos", "left", CVAR_ARCHIVE);
	cg_rpg_x = Cvar_Get ("cg_rpg_x", "14", CVAR_ARCHIVE);
	cg_rpg_y = Cvar_Get ("cg_rpg_y", "14", CVAR_ARCHIVE);
	cg_rpg_level   = Cvar_Get ("cg_rpg_level",   "1",                        CVAR_ROM);
	cg_rpg_xp      = Cvar_Get ("cg_rpg_xp",      "0",                        CVAR_ROM);
	cg_rpg_xp_max  = Cvar_Get ("cg_rpg_xp_max",  "1000",                     CVAR_ROM);
	cg_rpg_fr      = Cvar_Get ("cg_rpg_fr",      "1000",                     CVAR_ROM);
	cg_rpg_avatar  = Cvar_Get ("cg_rpg_avatar",  "gfx/rpg_hud/avatar_default", 0);
	cg_rpg_name    = Cvar_Get ("cg_rpg_name",    "",                         CVAR_ROM);
	cg_rpg_rank    = Cvar_Get ("cg_rpg_rank",    "Padawan",                  CVAR_ROM);
	cg_drawLeaderboard = Cvar_Get ("cg_drawLeaderboard", "0", 0);
	cg_drawStats = Cvar_Get ("cg_drawStats", "0", 0);
	cg_drawBounty = Cvar_Get ("cg_drawBounty", "0", 0);
	cg_drawShop = Cvar_Get ("cg_drawShop", "0", 0);
	cg_drawQuest = Cvar_Get ("cg_drawQuest", "0", 0);
	cg_drawInventory = Cvar_Get ("cg_drawInventory", "0", 0);
	cg_drawAch = Cvar_Get ("cg_drawAch", "0", 0);
	cg_drawTopCredits = Cvar_Get ("cg_drawTopCredits", "0", 0);
	cg_drawTopPotato = Cvar_Get ("cg_drawTopPotato", "0", 0);
	cg_drawAdv = Cvar_Get ("cg_drawAdv", "0", 0);

	Cmd_AddCommand( "rpg_hud_style", SCR_RPGHUDStyle_f, "Select RPG HUD style: classic (0) or bottom (1)" );
	Cmd_AddCommand( "rpg_hud_pos", SCR_RPGHUDPos_f, "Position RPG HUD: left, right, bottomright, bottomleft, bottomcenter" );
	Cmd_AddCommand( "hudsettings", SCR_HUDSettings_f, "Toggle HUD settings configuration studio overlay" );
	Cmd_AddCommand( "rpgmenu", SCR_RPGMenu_f, "Toggle Master RPG Hub menu overlay" );
	Cmd_AddCommand( "rpgui_test", SCR_RPGUITest_f, "Fill the RPG HUD with sample data: rpgui_test [name] [style]" );
	Cmd_AddCommand( "rpgxp_test", SCR_RPGXPTest_f, "Add XP to the sample HUD: rpgxp_test [amount]" );
	Cmd_AddCommand( "colui_test", SCR_ColUITest_f, "Open the collection book with sample data" );
	Cmd_AddCommand( "uiscreen_test", SCR_UIScreenTest_f, "Open a UI window with sample data: uiscreen_test <lb|stats|wanted|bounties|shop|inv|quest|ach|credits|potato|adv|menu|toast>" );
	Cmd_AddCommand( "menu", SCR_RPGMenu_f, "Toggle Master RPG Hub menu overlay" );
	Cmd_AddCommand( "settings", SCR_RPGMenu_f, "Toggle Master RPG Hub menu overlay" );
	Cmd_AddCommand( "partymenu", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "party", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "partystudio", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "adminmenu", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );
	Cmd_AddCommand( "admin", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );
	Cmd_AddCommand( "adminpanel", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );
	Cmd_AddCommand( "games", SCR_Games_f, "Toggle Galactic Cantina Games Hub overlay" );
	Cmd_AddCommand( "gamesmenu", SCR_Games_f, "Toggle Galactic Cantina Games Hub overlay" );
	Cmd_AddCommand( "blackjack", SCR_Blackjack_f, "Open Canto Bight Blackjack 21 table" );
	Cmd_AddCommand( "casino", SCR_Blackjack_f, "Open Canto Bight Blackjack 21 table" );
	Cmd_AddCommand( "pazaak", SCR_Pazaak_f, "Open Star Wars Pazaak games table" );

	scr_initialized = qtrue;
}

/*
==================
SCR_FillRoundedRect

Draws a rectangle with smooth rounded corners in 640x480 virtual coordinates
==================
*/
static void SCR_DrawRoundedGlassPanel( float x, float y, float w, float h, float r, const float *bgColor, const float *borderColor ) {
	if ( w <= 0 || h <= 0 ) return;

	// Fill simple translucent background rect
	if ( bgColor ) {
		SCR_FillRect( x, y, w, h, bgColor );
	}

	// Draw clean 1px border lines on all 4 sides
	if ( borderColor ) {
		SCR_FillRect( x, y, w, 1.0f, borderColor );          // Top
		SCR_FillRect( x, y + h - 1.0f, w, 1.0f, borderColor ); // Bottom
		SCR_FillRect( x, y, 1.0f, h, borderColor );          // Left
		SCR_FillRect( x + w - 1.0f, y, 1.0f, h, borderColor ); // Right
	}
}

qboolean SCR_NameMatch( const char *csName, const char *clName ) {
	char cleanCs[64], cleanCl[64];
	int csIdx = 0;
	for ( int j = 0; csName[j] && csIdx < 63; j++ ) {
		if ( csName[j] == '^' && csName[j+1] ) {
			j++;
			continue;
		}
		cleanCs[csIdx++] = csName[j];
	}
	cleanCs[csIdx] = 0;

	int clIdx = 0;
	for ( int j = 0; clName[j] && clIdx < 63; j++ ) {
		if ( clName[j] == '^' && clName[j+1] ) {
			j++;
			continue;
		}
		cleanCl[clIdx++] = clName[j];
	}
	cleanCl[clIdx] = 0;

	return (qboolean)(Q_stricmp( cleanCs, cleanCl ) == 0);
}

int SCR_GetPlayersCSBase( void ) {
	const char *localName = Cvar_VariableString( "name" );
	if ( localName && localName[0] ) {
		for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
			if ( !cl.gameState.stringOffsets[i] ) continue;
			const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[i];
			if ( cInfo && cInfo[0] == 'n' && cInfo[1] == '\\' ) {
				char nameBuf[64];
				Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );
				if ( nameBuf[0] && SCR_NameMatch( nameBuf, localName ) ) {
					int base = i - cl.snap.ps.clientNum;
					if ( base >= 0 && base < MAX_CONFIGSTRINGS ) {
						return base;
					}
				}
			}
		}
	}
	// Fallback to first player configstring
	for ( int i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		if ( !cl.gameState.stringOffsets[i] ) continue;
		const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		if ( cInfo && cInfo[0] == 'n' && cInfo[1] == '\\' ) {
			return i;
		}
	}
	return CS_PLAYERS;
}

int SCR_GetClientNumByName( const char *name ) {
	int csBase = SCR_GetPlayersCSBase();
	char cleanSearch[64];
	Q_strncpyz( cleanSearch, name, sizeof( cleanSearch ) );
	Q_CleanStr( cleanSearch );

	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( i + csBase >= MAX_CONFIGSTRINGS ) break;
		if ( !cl.gameState.stringOffsets[csBase + i] ) continue;
		const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
		if ( !cInfo || !cInfo[0] ) continue;

		char nameBuf[64];
		Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );
		Q_CleanStr( nameBuf );

		if ( nameBuf[0] && !Q_stricmp( nameBuf, cleanSearch ) ) {
			return i;
		}
	}
	return -1;
}

/*
==================
SCR_DrawVirtualString

Draws crisp text at 640x480 virtual coordinates matching SCR_FillRect & SCR_DrawPic
==================
*/
static void SCR_DrawVirtualString( float x, float y, float charSize, const char *string, const float *setColor ) {
	const char *s = string;
	float xx = x;
	vec4_t color;
	if ( setColor ) {
		Com_Memcpy( color, setColor, sizeof( color ) );
	} else {
		color[0] = 1.0f; color[1] = 1.0f; color[2] = 1.0f; color[3] = 1.0f;
	}

	const float baseAlpha = color[3] * ( s_uiAnimating ? s_uiAlpha : 1.0f );
	if ( s_uiAnimating ) {
		color[3] = baseAlpha;
		y += s_uiDY;
	}

	int font = SCR_UIFont();
	if ( font && !SCR_HasEmojiGlyph( string ) ) {
		float rh = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
		re->Font_DrawString( (int)( x + 0.5f ), (int)( y - rh * 0.20f + 0.5f ), string, color, font, -1, SCR_UIFontScale( font, charSize ) );
		re->SetColor( NULL );
		return;
	}

	re->SetColor( color );

	float renderH = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
	float advance = renderH * 0.50f;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			Com_Memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
			color[3] = baseAlpha;
			re->SetColor( color );
			s += 2;
			continue;
		}
		SCR_DrawChar( (int)xx, (int)y, renderH, *s );
		xx += advance;
		s++;
	}
	re->SetColor( NULL );
}

/*
==================
SCR_DrawVirtualStringWrapped

Renders a virtual string with automatic word wrapping within a max width, returning next Y position.
==================
*/
static float SCR_DrawVirtualStringWrapped( float x, float y, float charSize, float maxW, const char *string, const float *setColor ) {
	char lineBuffer[1024];
	const char *words = string;
	float yy = y;
	float renderH = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
	float charW = renderH * 0.50f;

	while ( *words ) {
		const char *lineStart = words;
		const char *lastWordEnd = words;
		float currentW = 0.0f;

		while ( *words ) {
			const char *wordEnd = words;
			while ( *wordEnd && *wordEnd != ' ' && *wordEnd != '\n' ) {
				wordEnd++;
			}

			int wordCharCount = 0;
			const char *w = words;
			while ( w < wordEnd ) {
				if ( Q_IsColorString( w ) ) {
					w += 2;
				} else {
					wordCharCount++;
					w++;
				}
			}
			float wordW = (float)wordCharCount * charW;

			if ( *words == '\n' ) {
				words++;
				break;
			}

			if ( currentW + wordW > maxW && currentW > 0.0f ) {
				break;
			}

			currentW += wordW;
			if ( *wordEnd == ' ' ) {
				currentW += charW;
				wordEnd++;
			}
			words = wordEnd;
			lastWordEnd = wordEnd;
		}

		int copyLen = lastWordEnd - lineStart;
		if ( copyLen > (int)sizeof(lineBuffer) - 1 ) copyLen = sizeof(lineBuffer) - 1;
		if ( copyLen < 0 ) copyLen = 0;
		Com_Memcpy( lineBuffer, lineStart, copyLen );
		lineBuffer[copyLen] = '\0';

		int len = strlen(lineBuffer);
		if ( len > 0 && lineBuffer[len - 1] == '\n' ) {
			lineBuffer[len - 1] = '\0';
		}

		SCR_DrawVirtualString( x, yy, charSize, lineBuffer, setColor );
		yy += charSize + 3.0f; // Line height spacing
	}
	return yy;
}

/*
==================
SCR_DrawMBIICapsule

Renders MBII-style metallic glass capsule matching bottom-left health & force gauges.
==================
*/
static void SCR_DrawMBIICapsule( float x, float y, float w, float h, const float *bgColor, const float *borderColor ) {
	if ( w <= 0 || h <= 0 ) return;

	// Fill dark metallic capsule body
	SCR_FillRect( x + 3.0f, y, w - 6.0f, h, bgColor );
	SCR_FillRect( x + 1.0f, y + 2.0f, 2.0f, h - 4.0f, bgColor );
	SCR_FillRect( x + w - 3.0f, y + 2.0f, 2.0f, h - 4.0f, bgColor );

	// Thin glowing cyan capsule border
	if ( borderColor ) {
		SCR_FillRect( x + 3.0f, y, w - 6.0f, 1.0f, borderColor );             // Top
		SCR_FillRect( x + 3.0f, y + h - 1.0f, w - 6.0f, 1.0f, borderColor ); // Bottom
		SCR_FillRect( x, y + 3.0f, 1.0f, h - 6.0f, borderColor );             // Left cap
		SCR_FillRect( x + w - 1.0f, y + 3.0f, 1.0f, h - 6.0f, borderColor ); // Right cap

		// Stepped corner caps
		SCR_FillRect( x + 1.0f, y + 1.0f, 2.0f, 1.0f, borderColor );
		SCR_FillRect( x + 1.0f, y + 1.0f, 1.0f, 2.0f, borderColor );

		SCR_FillRect( x + w - 3.0f, y + 1.0f, 2.0f, 1.0f, borderColor );
		SCR_FillRect( x + w - 2.0f, y + 1.0f, 1.0f, 2.0f, borderColor );

		SCR_FillRect( x + 1.0f, y + h - 2.0f, 2.0f, 1.0f, borderColor );
		SCR_FillRect( x + 1.0f, y + h - 3.0f, 1.0f, 2.0f, borderColor );

		SCR_FillRect( x + w - 3.0f, y + h - 2.0f, 2.0f, 1.0f, borderColor );
		SCR_FillRect( x + w - 2.0f, y + h - 3.0f, 1.0f, 2.0f, borderColor );
	}
}

/*
==================
SCR_DrawJediVectorEmblem

Renders sharp Jedi Order lightsaber wings emblem inside a circular ring.
==================
*/
static void SCR_DrawJediVectorEmblem( float cx, float cy, float radius ) {
	vec4_t ringCyan  = { 0.00f, 0.70f, 1.00f, 0.75f };
	vec4_t innerBg   = { 0.04f, 0.10f, 0.20f, 0.65f };
	vec4_t saberGold = { 1.00f, 0.85f, 0.20f, 0.95f };
	vec4_t saberCyan = { 0.20f, 0.90f, 1.00f, 0.95f };

	// Inner dark circle fill
	SCR_FillRect( cx - radius + 2.0f, cy - radius + 2.0f, (radius - 2.0f) * 2.0f, (radius - 2.0f) * 2.0f, innerBg );

	// Circular ring border
	SCR_FillRect( cx - radius + 3.0f, cy - radius, radius * 2.0f - 6.0f, 1.0f, ringCyan );
	SCR_FillRect( cx - radius + 3.0f, cy + radius - 1.0f, radius * 2.0f - 6.0f, 1.0f, ringCyan );
	SCR_FillRect( cx - radius, cy - radius + 3.0f, 1.0f, radius * 2.0f - 6.0f, ringCyan );
	SCR_FillRect( cx + radius - 1.0f, cy - radius + 3.0f, 1.0f, radius * 2.0f - 6.0f, ringCyan );

	// Jedi Lightsaber Wings & Central Blade
	SCR_FillRect( cx - 1.0f, cy - radius + 3.0f, 2.0f, radius * 2.0f - 6.0f, saberCyan ); // Vertical blade
	SCR_FillRect( cx - 5.0f, cy, 10.0f, 1.5f, saberGold );                                // Crossguard
	SCR_FillRect( cx - 7.0f, cy - 3.0f, 3.0f, 1.5f, saberGold );                          // Left wing tip
	SCR_FillRect( cx + 4.0f, cy - 3.0f, 3.0f, 1.5f, saberGold );                          // Right wing tip
	SCR_FillRect( cx - 3.0f, cy + 3.0f, 6.0f, 1.5f, saberGold );                          // Hilt base
}

/*
==================
SCR_DrawRPGHUDOverlay

Renders client-side RPG HUD Overlay (Supports Style 0 Classic & Style 1 Bottom Sleek Bar)
==================
*/
static float s_visualXP = -1.0f;
static int s_lastState = -1;
static qhandle_t s_hAvatar = 0;
static qhandle_t s_hAvatarFrame = 0;
static qhandle_t s_hModalBg = 0;
static qhandle_t s_hInventoryBg = 0;
static qhandle_t s_hWantedBg = 0;
static qhandle_t s_hShopBg = 0;
static qhandle_t s_hQuestBg = 0;
static qhandle_t s_hAchBg = 0;
static qhandle_t s_hTopBg = 0;
static qhandle_t s_hAdvBg = 0;
static qhandle_t s_hPotatoPic = 0;
static qhandle_t s_hBuyBtn = 0;
static qhandle_t s_hSellBtn = 0;
static qhandle_t s_hUseBtn = 0;
static qhandle_t s_hStatsCard = 0;
static qhandle_t s_hSettingsBg = 0;
static qhandle_t s_hSettingsBtnNormal = 0;
static qhandle_t s_hSettingsBtnHover = 0;
static qhandle_t s_hSettingsSliderTrack = 0;
static qhandle_t s_hSettingsSliderThumb = 0;
static qhandle_t s_hGlobalCursor = 0;

// Shortens a string with ".." so it fits in maxW (never cuts a ^ color code in half)
static void SCR_FitText( const char *in, char *out, int outSize, float maxW, float size ) {
	Q_strncpyz( out, in, outSize );
	if ( SCR_GetStringWidth( out, size ) <= maxW ) return;
	int len = (int)strlen( out );
	while ( len > 1 ) {
		len--;
		if ( len > 0 && out[len - 1] == '^' ) len--;
		out[len] = 0;
		char tmp[160];
		Com_sprintf( tmp, sizeof( tmp ), "%s..", out );
		if ( SCR_GetStringWidth( tmp, size ) <= maxW ) {
			Q_strncpyz( out, tmp, outSize );
			return;
		}
	}
}

// ---- Code-drawn UI kit (Holo look) --------------------------------------------------------------
// Fills a rectangle with the top-left and bottom-right corners cut at 45 degrees (one row per screen pixel)
static void SCR_FillChamfer( float x, float y, float w, float h, float cut, const float *color ) {
	int n = (int)ceilf( cut * 2.25f );
	if ( n < 1 ) n = 1;
	const float step = cut / (float)n;
	SCR_FillRect( x, y + cut, w, h - 2.0f * cut, color );
	for ( int i = 0; i < n; i++ ) {
		const float inset = cut - step * ( (float)i + 0.5f );
		SCR_FillRect( x + inset, y + step * (float)i, w - inset, step, color );
		SCR_FillRect( x, y + h - step * (float)( i + 1 ), w - inset, step, color );
	}
}

// 1px outline of the same shape (does not tint the inside of the panel)
static void SCR_StrokeChamfer( float x, float y, float w, float h, float cut, const float *color ) {
	int n = (int)ceilf( cut * 2.25f );
	if ( n < 1 ) n = 1;
	const float step = cut / (float)n;
	SCR_FillRect( x + cut, y, w - cut, 1.0f, color );
	SCR_FillRect( x, y + h - 1.0f, w - cut, 1.0f, color );
	SCR_FillRect( x, y + cut, 1.0f, h - cut, color );
	SCR_FillRect( x + w - 1.0f, y, 1.0f, h - cut, color );
	for ( int i = 0; i < n; i++ ) {
		const float inset = cut - step * ( (float)i + 0.5f );
		SCR_FillRect( x + inset - 0.3f, y + step * (float)i, 1.3f, step, color );
		SCR_FillRect( x + w - inset - 1.0f, y + h - step * (float)( i + 1 ), 1.3f, step, color );
	}
}

// Dark navy glass panel with a thin border and an accent bar on the left. cut = 0 gives square corners.
static void SCR_UIPanel( float x, float y, float w, float h, float cut, const float *bg, const float *border, const float *accent ) {
	if ( cut > 0.0f ) {
		SCR_FillChamfer( x, y, w, h, cut, bg );
		SCR_StrokeChamfer( x, y, w, h, cut, border );
		if ( accent ) SCR_FillRect( x, y + cut, 2.0f, h - 2.0f * cut, accent );
	} else {
		SCR_FillRect( x, y, w, h, bg );
		SCR_FillRect( x, y, w, 1.0f, border );
		SCR_FillRect( x, y + h - 1.0f, w, 1.0f, border );
		SCR_FillRect( x, y, 1.0f, h, border );
		SCR_FillRect( x + w - 1.0f, y, 1.0f, h, border );
		if ( accent ) SCR_FillRect( x, y, 3.0f, h, accent );
	}
}

// Small bordered key / button chip with centred text
static void SCR_UIChip( float x, float y, float w, float h, const char *text, float size, const float *fill, const float *border, const float *textCol ) {
	SCR_FillRect( x, y, w, h, fill );
	SCR_FillRect( x, y, w, 1.0f, border );
	SCR_FillRect( x, y + h - 1.0f, w, 1.0f, border );
	SCR_FillRect( x, y, 1.0f, h, border );
	SCR_FillRect( x + w - 1.0f, y, 1.0f, h, border );
	SCR_DrawVirtualString( x + ( w - SCR_GetStringWidth( text, size ) ) * 0.5f, y + ( h - size * 1.2f ) * 0.5f, size, text, textCol );
}

// Window title: coloured title text with a tinted rule underneath
static void SCR_UITitle( float x, float y, float w, const char *title, float size, const float *tint ) {
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( x, y, size, title, white );
	const float ly = y + size * 1.5f + 3.0f;
	vec4_t line = { tint[0], tint[1], tint[2], 0.85f };
	SCR_FillRect( x, ly, w, 1.0f, line );
	// a bright highlight travels along the rule
	const float seg = 46.0f;
	float sx0 = x + fmodf( (float)cls.realtime * 0.09f, w + seg ) - seg;
	float sx1 = sx0 + seg;
	if ( sx0 < x ) sx0 = x;
	if ( sx1 > x + w ) sx1 = x + w;
	if ( sx1 > sx0 ) {
		vec4_t glow = { 1.0f, 1.0f, 1.0f, 0.75f };
		SCR_FillRect( sx0, ly - 0.5f, sx1 - sx0, 2.0f, glow );
	}
}

// ---- Animation + widgets for the modal windows ---------------------------------------------------
static int s_uiShownMs[16];
static int s_uiLastDrawMs[16];
static int s_uiAnimMs = 0;
static float s_uiBaseAlpha = 1.0f;
static float s_uiBaseDY = 0.0f;

static float SCR_UIEase( float t ) {
	if ( t < 0.0f ) t = 0.0f;
	if ( t > 1.0f ) t = 1.0f;
	return 1.0f - ( 1.0f - t ) * ( 1.0f - t ) * ( 1.0f - t );
}

// Eased 0..1 progress since the window opened, after delayMs, over durMs
static float SCR_UIOpenEase( float delayMs, float durMs ) {
	return SCR_UIEase( ( (float)s_uiAnimMs - delayMs ) / durMs );
}

// Starts the open animation (fade + slide down) for the overlay in this slot; call SCR_UIAnimEnd after it was drawn
static void SCR_UIAnimBegin( int slot ) {
	const int now = cls.realtime;
	if ( now - s_uiLastDrawMs[slot] > 250 ) {
		s_uiShownMs[slot] = now;	// (re)opened
	}
	s_uiAnimMs = now - s_uiShownMs[slot];
	const float e = SCR_UIEase( (float)s_uiAnimMs / 260.0f );
	s_uiAlpha = e;
	s_uiDY = -( 1.0f - e ) * 16.0f;
	s_uiBaseAlpha = s_uiAlpha;
	s_uiBaseDY = s_uiDY;
	s_uiAnimating = qtrue;
	s_uiDrew = qfalse;
}

static void SCR_UIAnimEnd( int slot ) {
	if ( s_uiDrew ) s_uiLastDrawMs[slot] = cls.realtime;
	s_uiAnimating = qfalse;
	s_uiAlpha = 1.0f;
	s_uiDY = 0.0f;
}

static void SCR_UIAnimated( int slot, void ( *fn )( void ) ) {
	SCR_UIAnimBegin( slot );
	fn();
	SCR_UIAnimEnd( slot );
}

// Fades and slides one row / block in; ms = time since it should start appearing
static void SCR_UIRowBeginMs( int ms, int i ) {
	const float e = SCR_UIEase( ( (float)ms - 140.0f - 40.0f * (float)i ) / 220.0f );
	s_uiAlpha = s_uiBaseAlpha * e;
	s_uiDY = s_uiBaseDY + ( 1.0f - e ) * 8.0f;
}

static void SCR_UIRowBegin( int i ) {
	SCR_UIRowBeginMs( s_uiAnimMs, i );
}

static void SCR_UIRowEnd( void ) {
	s_uiAlpha = s_uiBaseAlpha;
	s_uiDY = s_uiBaseDY;
}

// Window: chamfered panel, title with a travelling highlight, optional ESC chip
static void SCR_UIWindow( float x, float y, float w, float h, const char *title, const float *tint, qboolean closeChip ) {
	vec4_t bg     = { 0.04f, 0.07f, 0.14f, 0.94f };
	vec4_t border = { tint[0] * 0.8f, tint[1] * 0.8f, tint[2] * 0.8f, 0.70f };
	SCR_UIPanel( x, y, w, h, 10.0f, bg, border, tint );
	SCR_UITitle( x + 18.0f, y + 10.0f, w - 36.0f, title, 7.0f, tint );
	if ( closeChip ) {
		vec4_t chipFill   = { 0.30f, 0.06f, 0.06f, 0.9f };
		vec4_t chipBorder = { 0.90f, 0.30f, 0.25f, 0.9f };
		vec4_t white      = { 1.0f, 1.0f, 1.0f, 1.0f };
		SCR_UIChip( x + w - 40.0f, y + 8.0f, 34.0f, 15.0f, "ESC", 3.6f, chipFill, chipBorder, white );
	}
}

static void SCR_UIFooter( float x, float y, float w, float h, const float *tint, const char *text ) {
	vec4_t line = { tint[0], tint[1], tint[2], 0.35f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_FillRect( x + 18.0f, y + h - 22.0f, w - 36.0f, 1.0f, line );
	SCR_DrawVirtualString( x + 18.0f, y + h - 16.0f, 3.8f, text, white );
}

// Button with a tinted fill, border and a left accent that lights up on hover
static void SCR_UIButton( float x, float y, float w, float h, const char *label, float size, qboolean hover, const float *tint ) {
	const float k = hover ? 0.55f : 0.28f;
	vec4_t fill   = { tint[0] * k, tint[1] * k, tint[2] * k, hover ? 0.95f : 0.85f };
	vec4_t border = { tint[0], tint[1], tint[2], hover ? 1.0f : 0.60f };
	vec4_t white  = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_FillRect( x, y, w, h, fill );
	SCR_FillRect( x, y, w, 1.0f, border );
	SCR_FillRect( x, y + h - 1.0f, w, 1.0f, border );
	SCR_FillRect( x, y, 1.0f, h, border );
	SCR_FillRect( x + w - 1.0f, y, 1.0f, h, border );
	if ( hover ) SCR_FillRect( x, y, 3.0f, h, border );
	SCR_DrawVirtualString( x + ( w - SCR_GetStringWidth( label, size ) ) * 0.5f, y + ( h - size * 1.2f ) * 0.5f, size, label, white );
}

// Gold / silver / bronze chip for the top three, plain number for the rest
static void SCR_UIRankBadge( float x, float y, float w, float h, int rank, float size ) {
	vec4_t chip = { 0.0f, 0.0f, 0.0f, 0.0f };
	if ( rank == 1 )      { chip[0] = 1.00f; chip[1] = 0.80f; chip[2] = 0.20f; chip[3] = 1.0f; }
	else if ( rank == 2 ) { chip[0] = 0.78f; chip[1] = 0.84f; chip[2] = 0.90f; chip[3] = 1.0f; }
	else if ( rank == 3 ) { chip[0] = 0.80f; chip[1] = 0.50f; chip[2] = 0.25f; chip[3] = 1.0f; }
	const char *rs = va( "%d", rank );
	const float tx = x + ( w - SCR_GetStringWidth( rs, size ) ) * 0.5f;
	const float ty = y + ( h - size * 1.2f ) * 0.5f;
	if ( chip[3] > 0.0f ) {
		vec4_t dark = { 0.05f, 0.05f, 0.08f, 1.0f };
		SCR_FillRect( x, y, w, h, chip );
		SCR_DrawVirtualString( tx, ty, size, rs, dark );
	} else {
		vec4_t num = { 0.75f, 0.85f, 1.0f, 1.0f };
		SCR_DrawVirtualString( tx, ty, size, rs, num );
	}
}

// XP-style progress bar: gradient fill, travelling sheen and a bright tip
static void SCR_UIBar( float x, float y, float w, float h, float ratio, const float *lo, const float *hi ) {
	if ( ratio < 0.0f ) ratio = 0.0f;
	if ( ratio > 1.0f ) ratio = 1.0f;
	vec4_t trackBorder = { 0.20f, 0.50f, 0.80f, 0.55f };
	vec4_t track       = { 0.02f, 0.04f, 0.09f, 0.95f };
	SCR_FillRect( x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, trackBorder );
	SCR_FillRect( x, y, w, h, track );
	const float fw = w * ratio;
	if ( fw > 0.5f ) {
		SCR_FillRect( x, y, fw, h, lo );
		SCR_FillRect( x, y, fw, h * 0.45f, hi );
		const float sweep = fmodf( (float)cls.realtime * 0.045f, w + 60.0f ) - 30.0f;
		float sx0 = x + sweep, sx1 = sx0 + 26.0f;
		if ( sx0 < x ) sx0 = x;
		if ( sx1 > x + fw ) sx1 = x + fw;
		if ( sx1 > sx0 ) {
			vec4_t sheen = { 1.0f, 1.0f, 1.0f, 0.35f };
			SCR_FillRect( sx0, y, sx1 - sx0, h, sheen );
		}
		vec4_t tip = { 1.0f, 1.0f, 1.0f, 0.9f };
		SCR_FillRect( x + fw - 1.5f, y, 1.5f, h, tip );
	}
}

// Slider: label above (clear of the thumb), thin track, filled part, narrow thumb centred on the value
static void SCR_UISlider( float x, float y, float w, float h, float progress, const char *label, const char *valueText ) {
	vec4_t dim   = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t track = { 0.05f, 0.10f, 0.18f, 1.0f };
	vec4_t fill  = { 0.10f, 0.60f, 0.90f, 1.0f };
	vec4_t thumb = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t thumbBorder = { 0.20f, 0.80f, 1.0f, 1.0f };
	if ( progress < 0.0f ) progress = 0.0f;
	if ( progress > 1.0f ) progress = 1.0f;
	SCR_DrawVirtualString( x, y - 9.5f, 3.6f, label, dim );
	SCR_DrawVirtualString( x + w - SCR_GetStringWidth( valueText, 3.6f ), y - 9.5f, 3.6f, valueText, dim );
	const float ty = y + h * 0.5f - 1.5f;
	SCR_FillRect( x, ty, w, 3.0f, track );
	SCR_FillRect( x, ty, w * progress, 3.0f, fill );
	float tx = x + w * progress - 3.0f;
	if ( tx < x ) tx = x;
	if ( tx > x + w - 6.0f ) tx = x + w - 6.0f;
	SCR_FillRect( tx - 1.0f, y - 2.0f, 8.0f, h + 4.0f, thumbBorder );
	SCR_FillRect( tx, y - 1.0f, 6.0f, h + 2.0f, thumb );
}

// Ranked list window (top wealth / top potato): rank badge, name, value column
static void SCR_UIDrawTopList( const char *title, const float *tint, const char *valueHead, int count, char names[][64], char values[][24], const char *footer ) {
	vec4_t dim   = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	const float winW = 290.0f, winH = 400.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 40.0f;
	SCR_UIWindow( winX, winY, winW, winH, title, tint, qtrue );

	const float headY = winY + 38.0f;
	const float colName = winX + 46.0f;
	const float rightEdge = winX + winW - 18.0f;
	SCR_DrawVirtualString( winX + 20.0f, headY, 3.6f, "#", dim );
	SCR_DrawVirtualString( colName, headY, 3.6f, "PLAYER", dim );
	SCR_DrawVirtualString( rightEdge - SCR_GetStringWidth( valueHead, 3.6f ), headY, 3.6f, valueHead, dim );

	const float rowH = 26.0f;
	const float rowStart = headY + 13.0f;
	for ( int i = 0; i < 10; i++ ) {
		const float ry = rowStart + (float)i * rowH;
		SCR_UIRowBegin( i );
		if ( i % 2 == 0 ) {
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, 0.05f };
			SCR_FillRect( winX + 12.0f, ry, winW - 24.0f, rowH - 2.0f, zebra );
		}
		if ( i < count ) {
			const float ty = ry + ( rowH - 2.0f - 4.8f * 1.2f ) * 0.5f;
			SCR_UIRankBadge( winX + 18.0f, ry + 3.0f, 20.0f, rowH - 8.0f, i + 1, 4.6f );
			char fit[64];
			SCR_FitText( names[i], fit, sizeof( fit ), rightEdge - colName - 70.0f, 4.8f );
			SCR_DrawVirtualString( colName, ty, 4.8f, va( "^7%s", fit ), white );
			SCR_DrawVirtualString( rightEdge - SCR_GetStringWidth( values[i], 4.8f ), ty, 4.8f, values[i], white );
		}
		SCR_UIRowEnd();
	}
	SCR_UIFooter( winX, winY, winW, winH, tint, footer );
}

// Frame for the older table / studio windows: chamfered panel, centred title with the travelling rule, ESC chip
static void SCR_UIGameFrame( float x, float y, float w, float h, const float *bg, const float *accent, const char *title, float size, float titleY ) {
	vec4_t tint   = { accent[0], accent[1], accent[2], 1.0f };
	vec4_t border = { accent[0] * 0.8f, accent[1] * 0.8f, accent[2] * 0.8f, 0.70f };
	vec4_t white  = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIPanel( x, y, w, h, 10.0f, bg, border, tint );
	SCR_DrawVirtualString( x + ( w - SCR_GetStringWidth( title, size ) ) * 0.5f, y + titleY, size, title, white );
	SCR_UITitle( x + 18.0f, y + titleY, w - 36.0f, "", size, tint );
	vec4_t chipFill   = { 0.30f, 0.06f, 0.06f, 0.9f };
	vec4_t chipBorder = { 0.90f, 0.30f, 0.25f, 0.9f };
	SCR_UIChip( x + w - 40.0f, y + 8.0f, 34.0f, 15.0f, "ESC", 3.6f, chipFill, chipBorder, white );
}

static void SCR_DrawStatsCaption( float x, float y, float size ) {
	vec4_t shadow = { 0.0f, 0.0f, 0.0f, 0.85f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	const char *plain = va( "%d Kills  %d Deaths  %d Wins  %d Losses", g_rpgStats.kills, g_rpgStats.deaths, g_rpgStats.wins, g_rpgStats.losses );
	SCR_DrawVirtualString( x + 0.8f, y + 0.8f, size, plain, shadow );
	SCR_DrawVirtualString( x, y, size, va( "^2%d Kills  ^1%d Deaths  ^2%d Wins  ^1%d Losses", g_rpgStats.kills, g_rpgStats.deaths, g_rpgStats.wins, g_rpgStats.losses ), white );
}

void SCR_DrawRPGHUDOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Register HD TGA Shaders dynamically whenever active
	if ( s_hModalBg <= 0 && re && re->RegisterShader ) {
		s_hAvatarFrame = re->RegisterShader( "gfx/rpg_hud/avatar_frame" );
		s_hModalBg     = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );
		s_hInventoryBg = re->RegisterShader( "gfx/rpg_hud/inventory_bg" );
		s_hUseBtn      = re->RegisterShader( "gfx/rpg_hud/use_btn" );
		s_hStatsCard   = re->RegisterShader( "gfx/rpg_hud/stats_card" );
	}

	if ( re && re->RegisterShader ) {
		const char *avPath = (cg_rpg_avatar && cg_rpg_avatar->string[0]) ? cg_rpg_avatar->string : "gfx/rpg_hud/avatar_default";
		s_hAvatar = re->RegisterShader( avPath );
	}




	int style = cg_rpg_style ? cg_rpg_style->integer : 0;
	if ( style < 0 || style > 1 ) style = 0;

	// Hide HUD when full screen menus/overlays are active
	if ( (cg_drawStats && cg_drawStats->integer) ||
		 (cg_drawLeaderboard && cg_drawLeaderboard->integer) ||
		 (cg_drawTopCredits && cg_drawTopCredits->integer) ||
		 (cg_drawTopPotato && cg_drawTopPotato->integer) ||
		 (cg_drawBounty && cg_drawBounty->integer) ||
		 (cg_drawShop && cg_drawShop->integer) ||
		 (cg_drawQuest && cg_drawQuest->integer) ||
		 (cg_drawInventory && cg_drawInventory->integer) ||
		 (cg_drawAch && cg_drawAch->integer) ||
		 (cg_drawAdv && cg_drawAdv->integer) ) {
		return;
	}

	cvar_t *clName = Cvar_Get( "name", "Padawan", 0 );
	const char *playerName = (cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : (clName ? clName->string : "Player");
	const char *rankTitle = (cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : "Padawan";

	float nameW = SCR_GetStringWidth( playerName, 5.2f );
	float titleW = SCR_GetStringWidth( rankTitle, 4.3f );
	float maxStrW = (nameW > titleW) ? nameW : titleW;
	float panelW, panelH;
	panelW = 200.0f;
	panelH = 48.0f;
	(void)maxStrW;

	// Preset position defaults
	float defaultX = 14.0f;
	float defaultY = 14.0f;

	if ( cg_rpg_pos && cg_rpg_pos->string[0] ) {
		if ( !Q_stricmp( cg_rpg_pos->string, "right" ) || !Q_stricmp( cg_rpg_pos->string, "topright" ) ) {
			defaultX = 640.0f - panelW - 14.0f; defaultY = 14.0f;
		} else if ( !Q_stricmp( cg_rpg_pos->string, "bottomright" ) ) {
			defaultX = 640.0f - panelW - 14.0f; defaultY = 345.0f;
		} else if ( !Q_stricmp( cg_rpg_pos->string, "bottomleft" ) ) {
			defaultX = 14.0f; defaultY = 345.0f;
		} else if ( !Q_stricmp( cg_rpg_pos->string, "bottomcenter" ) || !Q_stricmp( cg_rpg_pos->string, "center" ) ) {
			defaultX = 320.0f - panelW * 0.5f; defaultY = 428.0f;
		}
	}

	float panelX = (cg_rpg_x && cg_rpg_x->value != 0.0f) ? cg_rpg_x->value : defaultX;
	float panelY = (cg_rpg_y && cg_rpg_y->value != 0.0f) ? cg_rpg_y->value : defaultY;

	// Synchronize level: prefer g_rpgStats.level if updated from server
	int level = (g_rpgStats.level > 0) ? g_rpgStats.level : ((cg_rpg_level && cg_rpg_level->integer > 0) ? cg_rpg_level->integer : 1);
	if ( level < 1 ) level = 1;

	int fr = cg_rpg_fr ? cg_rpg_fr->integer : 1000;
	int relativeXP = cg_rpg_xp ? cg_rpg_xp->integer : 0;
	int xpMax = (cg_rpg_xp_max && cg_rpg_xp_max->integer > 0) ? cg_rpg_xp_max->integer : 1000;

	if ( relativeXP < 0 ) relativeXP = 0;
	if ( relativeXP > xpMax ) relativeXP = xpMax;

	if ( s_visualXP < 0.0f ) {
		s_visualXP = (float)relativeXP;
	} else {
		float diff = (float)relativeXP - s_visualXP;
		if ( fabsf( diff ) > 0.1f ) {
			s_visualXP += diff * 0.08f;
		} else {
			s_visualXP = (float)relativeXP;
		}
	}

	float xpRatio = s_visualXP / (float)xpMax;
	if ( xpRatio < 0.0f ) xpRatio = 0.0f;
	if ( xpRatio > 1.0f ) xpRatio = 1.0f;

	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	re->SetColor( whiteColor );

	// ========================================================
	// STYLE 4: HOLO CARD - code-drawn, same look as the duel result announcer.
	// Slides in when it appears; the XP bar eases to its value, has a moving sheen and flashes on every XP gain.
	// ========================================================
	if ( style == 0 || style == 1 ) {
		const qboolean edge = ( style == 1 ) ? qtrue : qfalse;	// same card with cut corners
		const float cut = 7.0f;
		static int s_holoShownMs = 0;
		static int s_holoLastDrawMs = 0;
		static int s_holoLastTotalXP = -1;
		static int s_holoLastLevel = -1;
		static int s_holoGainMs = -100000;
		static int s_holoGain = 0;
		static int s_holoLevelUpMs = -100000;

		const int now = cls.realtime;
		if ( now - s_holoLastDrawMs > 400 ) {
			s_holoShownMs = now;	// (re)appearing: slide in again
		}
		s_holoLastDrawMs = now;

		// slide in from the left with a cubic ease-out
		float t = (float)( now - s_holoShownMs ) / 450.0f;
		if ( t < 0.0f ) t = 0.0f;
		if ( t > 1.0f ) t = 1.0f;
		const float ease = 1.0f - ( 1.0f - t ) * ( 1.0f - t ) * ( 1.0f - t );
		const float a = ease;
		panelX -= ( 1.0f - ease ) * ( panelW + 20.0f );

		// XP gain / level up detection
		const int totalXP = ( g_rpgStats.xp > 0 ) ? g_rpgStats.xp : relativeXP;
		if ( s_holoLastLevel >= 0 && level > s_holoLastLevel ) {
			s_holoLevelUpMs = now;
			s_visualXP = 0.0f;	// the bar restarts instead of draining backwards
		}
		if ( s_holoLastTotalXP >= 0 && totalXP > s_holoLastTotalXP ) {
			s_holoGain = totalXP - s_holoLastTotalXP;
			s_holoGainMs = now;
		}
		s_holoLastTotalXP = totalXP;
		s_holoLastLevel = level;

		float ratio = s_visualXP / (float)xpMax;
		if ( ratio < 0.0f ) ratio = 0.0f;
		if ( ratio > 1.0f ) ratio = 1.0f;

		const int levelAge = now - s_holoLevelUpMs;
		const qboolean levelUp = ( levelAge >= 0 && levelAge < 2200 ) ? qtrue : qfalse;
		const float lvPulse = levelUp ? ( 0.5f + 0.5f * sinf( (float)levelAge * 0.012f ) ) : 0.0f;

		// panel: dark navy glass, 1px border, accent bar on the left
		vec4_t bg = { 0.04f, 0.07f, 0.14f, 0.88f * a };
		vec4_t border = { 0.20f, 0.50f, 0.80f, 0.55f * a };
		if ( levelUp ) { border[0] = 0.95f; border[1] = 0.78f; border[2] = 0.20f; border[3] = ( 0.55f + 0.4f * lvPulse ) * a; }
		vec4_t accent = { 0.10f, 0.75f, 0.95f, a };
		if ( levelUp ) { accent[0] = 1.0f; accent[1] = 0.80f; accent[2] = 0.20f; }
		SCR_UIPanel( panelX, panelY, panelW, panelH, edge ? cut : 0.0f, bg, border, accent );

		// avatar
		const float avSize = 34.0f;
		const float avX = panelX + 8.0f;
		const float avY = panelY + ( panelH - avSize ) * 0.5f;
		{
			vec4_t avFrame = { 0.20f, 0.50f, 0.80f, 0.70f * a };
			SCR_FillRect( avX - 1.0f, avY - 1.0f, avSize + 2.0f, avSize + 2.0f, avFrame );
			vec4_t avBg = { 0.03f, 0.06f, 0.11f, a };
			SCR_FillRect( avX, avY, avSize, avSize, avBg );
			if ( s_hAvatar ) {
				vec4_t av = { 1.0f, 1.0f, 1.0f, a };
				re->SetColor( av );
				SCR_DrawPic( avX, avY, avSize, avSize, s_hAvatar );
				re->SetColor( NULL );
			} else {
				SCR_DrawJediVectorEmblem( avX + avSize * 0.5f, avY + avSize * 0.5f, avSize * 0.5f );
			}
		}

		const float textX = avX + avSize + 8.0f;
		const float rightX = panelX + panelW - 8.0f;
		vec4_t whiteA = { 1.0f, 1.0f, 1.0f, a };

		// row 1: name (shortened to fit) and level badge pinned right
		char lvStr[32];
		Com_sprintf( lvStr, sizeof( lvStr ), "^3Lv %d", level );
		const float nameSize = 5.0f;
		const float row1Y = panelY + 4.0f;
		const float row2Y = panelY + 16.0f;
		const float lvW = SCR_GetStringWidth( lvStr, nameSize );
		char nameFit[96];
		SCR_FitText( playerName, nameFit, sizeof( nameFit ), rightX - textX - lvW - 8.0f, nameSize );
		SCR_DrawVirtualString( textX, row1Y, nameSize, va( "^7%s", nameFit ), whiteA );
		SCR_DrawVirtualString( rightX - lvW, row1Y, nameSize, lvStr, whiteA );

		// row 2: rank and Elo on the left, XP numbers on the right (or LEVEL UP while it is flashing)
		SCR_DrawVirtualString( textX, row2Y, 3.4f, va( "^3%s ^7| ^2%d Elo", rankTitle, fr ), whiteA );
		if ( levelUp && ( levelAge / 250 ) % 2 == 0 ) {
			const char *lu = "^3LEVEL UP!";
			SCR_DrawVirtualString( rightX - SCR_GetStringWidth( lu, 3.4f ), row2Y, 3.4f, lu, whiteA );
		} else {
			const char *xs = va( "^7%d ^5/ %d XP", relativeXP, xpMax );
			SCR_DrawVirtualString( rightX - SCR_GetStringWidth( xs, 3.2f ), row2Y, 3.2f, xs, whiteA );
		}

		// row 3: kills / deaths / wins / losses, spelled out, inside the card
		{
			vec4_t statsCol = { 1.0f, 1.0f, 1.0f, a };
			SCR_DrawVirtualString( textX, panelY + 27.0f, 3.0f,
				va( "^2%d Kills  ^1%d Deaths  ^2%d Wins  ^1%d Losses", g_rpgStats.kills, g_rpgStats.deaths, g_rpgStats.wins, g_rpgStats.losses ), statsCol );
		}

		// row 4: the animated XP bar
		const float barX = textX;
		const float barY = panelY + panelH - 12.0f;
		const float barW = rightX - barX;
		const float barH = 6.0f;
		vec4_t trackBorder = { 0.20f, 0.50f, 0.80f, 0.55f * a };
		vec4_t track = { 0.02f, 0.04f, 0.09f, 0.95f * a };
		SCR_FillRect( barX - 1.0f, barY - 1.0f, barW + 2.0f, barH + 2.0f, trackBorder );
		SCR_FillRect( barX, barY, barW, barH, track );

		const float fillW = barW * ratio;
		if ( fillW > 0.5f ) {
			vec4_t fillLo = { 0.08f, 0.55f, 0.90f, a };
			vec4_t fillHi = { 0.30f, 0.85f, 1.00f, a };
			if ( levelUp ) { fillLo[0] = 0.90f; fillLo[1] = 0.65f; fillLo[2] = 0.10f; fillHi[0] = 1.0f; fillHi[1] = 0.88f; fillHi[2] = 0.35f; }
			SCR_FillRect( barX, barY, fillW, barH, fillLo );
			SCR_FillRect( barX, barY, fillW, barH * 0.45f, fillHi );

			// sheen: a soft band sweeping across the filled part every ~2.4s
			const float sweep = fmodf( (float)now * 0.045f, barW + 60.0f ) - 30.0f;
			float sx0 = barX + sweep, sx1 = sx0 + 26.0f;
			if ( sx0 < barX ) sx0 = barX;
			if ( sx1 > barX + fillW ) sx1 = barX + fillW;
			if ( sx1 > sx0 ) {
				vec4_t sheen = { 1.0f, 1.0f, 1.0f, 0.35f * a };
				SCR_FillRect( sx0, barY, sx1 - sx0, barH, sheen );
			}

			// bright tip at the end of the fill
			vec4_t tip = { 1.0f, 1.0f, 1.0f, 0.9f * a };
			SCR_FillRect( barX + fillW - 1.5f, barY, 1.5f, barH, tip );
		}

		// flash over the bar and a floating "+N XP" after every XP gain
		const int gainAge = now - s_holoGainMs;
		if ( gainAge >= 0 && gainAge < 1400 ) {
			if ( gainAge < 700 && fillW > 0.5f ) {
				vec4_t flash = { 1.0f, 1.0f, 1.0f, 0.55f * ( 1.0f - (float)gainAge / 700.0f ) * a };
				SCR_FillRect( barX, barY, fillW, barH, flash );
			}
			const float ga = 1.0f - (float)gainAge / 1400.0f;
			vec4_t gcol = { 1.0f, 1.0f, 1.0f, ga * a };
			const char *gs = va( "^3+%d XP", s_holoGain );
			SCR_DrawVirtualString( rightX - SCR_GetStringWidth( gs, 4.2f ), panelY - 8.0f - 8.0f * ( 1.0f - ga ), 4.2f, gs, gcol );
		}

		// quarter ticks on the bar
		vec4_t tick = { 0.02f, 0.04f, 0.09f, 0.8f * a };
		for ( int k = 1; k < 4; k++ ) {
			SCR_FillRect( barX + barW * 0.25f * k, barY, 1.0f, barH, tick );
		}

		return;
	}

}

topLeaderboardEntry_t g_topLeaderboard[10];
int g_topLeaderboardCount = 0;

/*
==================
SCR_DrawLeaderboardOverlay

Renders sleek modal popup window showing top 10 ranked players with smooth HD glass backdrop
==================
*/
void SCR_DrawLeaderboardOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawLeaderboard || !cg_drawLeaderboard->integer ) return;

	const float winW = 290.0f, winH = 400.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 40.0f;
	vec4_t tint  = { 0.20f, 0.70f, 1.00f, 1.0f };
	vec4_t dim   = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^5DUELIST ^7LEADERBOARD", tint, qtrue );

	const float headY = winY + 38.0f;
	const float colName = winX + 46.0f;
	const float colLv = winX + 146.0f;
	const float colTitle = winX + 174.0f;
	const float rightEdge = winX + winW - 18.0f;
	SCR_DrawVirtualString( winX + 20.0f, headY, 3.6f, "#", dim );
	SCR_DrawVirtualString( colName, headY, 3.6f, "PLAYER", dim );
	SCR_DrawVirtualString( colLv, headY, 3.6f, "LV", dim );
	SCR_DrawVirtualString( colTitle, headY, 3.6f, "TITLE", dim );
	SCR_DrawVirtualString( rightEdge - SCR_GetStringWidth( "ELO", 3.6f ), headY, 3.6f, "ELO", dim );

	const float rowH = 26.0f;
	const float rowStart = headY + 13.0f;
	for ( int i = 0; i < 10; i++ ) {
		const float ry = rowStart + (float)i * rowH;
		SCR_UIRowBegin( i );
		if ( i % 2 == 0 ) {
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, 0.05f };
			SCR_FillRect( winX + 12.0f, ry, winW - 24.0f, rowH - 2.0f, zebra );
		}
		if ( i < g_topLeaderboardCount ) {
			topLeaderboardEntry_t *e = &g_topLeaderboard[i];
			const float ty = ry + ( rowH - 2.0f - 4.8f * 1.2f ) * 0.5f;
			SCR_UIRankBadge( winX + 18.0f, ry + 3.0f, 20.0f, rowH - 8.0f, i + 1, 4.6f );
			char fit[64];
			SCR_FitText( e->displayName, fit, sizeof( fit ), colLv - colName - 6.0f, 4.8f );
			SCR_DrawVirtualString( colName, ty, 4.8f, va( "^7%s", fit ), white );
			SCR_DrawVirtualString( colLv, ty, 4.8f, va( "^3%d", e->level ), white );
			SCR_DrawVirtualString( colTitle, ty, 4.2f, va( "^3%.12s", e->rankTitle ), white );
			const char *eloStr = va( "^2%d", e->fr );
			SCR_DrawVirtualString( rightEdge - SCR_GetStringWidth( eloStr, 4.8f ), ty, 4.8f, eloStr, white );
		}
		SCR_UIRowEnd();
	}
	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Press ^1ESC^7 to close the Duelist Leaderboard" );
}

/*
==================
SCR_DrawStatsOverlay

Renders sleek modal popup stats sheet card with full player statistics
==================
*/
void SCR_DrawStatsOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawStats || !cg_drawStats->integer ) return;
	if ( !g_rpgStats.active ) return;

	const float winW = 290.0f, winH = 400.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 40.0f;
	vec4_t tint  = { 0.95f, 0.75f, 0.20f, 1.0f };
	vec4_t dim   = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^3PLAYER ^7STATISTICS", tint, qtrue );

	// profile: avatar + name / rank / level
	const float avX = winX + 20.0f, avY = winY + 40.0f, avS = 62.0f;
	vec4_t avFrame = { 0.20f, 0.50f, 0.80f, 0.80f };
	vec4_t avBg    = { 0.03f, 0.06f, 0.11f, 1.0f };
	SCR_FillRect( avX - 1.0f, avY - 1.0f, avS + 2.0f, avS + 2.0f, avFrame );
	SCR_FillRect( avX, avY, avS, avS, avBg );
	qhandle_t hAv = s_hAvatar;
	if ( !hAv ) hAv = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
	if ( hAv ) SCR_DrawPic( avX, avY, avS, avS, hAv );

	const float px = avX + avS + 14.0f;
	SCR_DrawVirtualString( px, avY + 1.0f, 6.0f, va( "^7%s", g_rpgStats.name[0] ? g_rpgStats.name : "Player" ), white );
	SCR_DrawVirtualString( px, avY + 17.0f, 4.8f, va( "^3%s", g_rpgStats.rankTitle[0] ? g_rpgStats.rankTitle : "Padawan" ), white );
	SCR_DrawVirtualString( px, avY + 31.0f, 4.6f, va( "^5Lv %d   ^7|   ^2%d Elo", g_rpgStats.level, g_rpgStats.elo ), white );

	// level progress bar (fills in when the window opens)
	const int perLevel = g_rpgXpPerLevel > 0 ? g_rpgXpPerLevel : 1000;
	const int curXP = g_rpgStats.xp % perLevel;
	SCR_DrawVirtualString( px, avY + 46.0f, 3.8f, va( "^5XP ^7%d / %d   ^5(total %d)", curXP, perLevel, g_rpgStats.xp ), white );
	vec4_t lo = { 0.08f, 0.55f, 0.90f, 1.0f };
	vec4_t hi = { 0.30f, 0.85f, 1.00f, 1.0f };
	SCR_UIBar( winX + 20.0f, avY + avS + 12.0f, winW - 40.0f, 7.0f, ( (float)curXP / (float)perLevel ) * SCR_UIOpenEase( 300.0f, 700.0f ), lo, hi );

	// table
	char rival[96];
	if ( g_rpgStats.topRivalCount > 0 ) Com_sprintf( rival, sizeof( rival ), "^1%s ^5(%d)", g_rpgStats.topRivalName, g_rpgStats.topRivalCount );
	else Q_strncpyz( rival, "^7None", sizeof( rival ) );
	const float wl = g_rpgStats.losses > 0 ? (float)g_rpgStats.wins / (float)g_rpgStats.losses : (float)g_rpgStats.wins;
	const float kd = g_rpgStats.deaths > 0 ? (float)g_rpgStats.kills / (float)g_rpgStats.deaths : (float)g_rpgStats.kills;
	const char *labels[7] = { "Credits", "Wins / Losses", "Kills / Deaths", "Highest streak", "Favourite weapon", "Trivia wins", "Main rival" };
	char vals[7][96];
	Com_sprintf( vals[0], sizeof( vals[0] ), "^3%d CR", g_rpgStats.credits );
	Com_sprintf( vals[1], sizeof( vals[1] ), "^2%d^7/^1%d ^5(%.2f)", g_rpgStats.wins, g_rpgStats.losses, wl );
	Com_sprintf( vals[2], sizeof( vals[2] ), "^2%d^7/^1%d ^5(%.2f)", g_rpgStats.kills, g_rpgStats.deaths, kd );
	Com_sprintf( vals[3], sizeof( vals[3] ), "^3%d", g_rpgStats.highestStreak );
	Com_sprintf( vals[4], sizeof( vals[4] ), "^7%s", g_rpgStats.favWeapon[0] ? g_rpgStats.favWeapon : "Lightsaber" );
	Com_sprintf( vals[5], sizeof( vals[5] ), "^7%d", g_rpgStats.triviaWins );
	Q_strncpyz( vals[6], rival, sizeof( vals[6] ) );

	const float rowH = 28.0f;
	const float tableY = winY + 138.0f;
	for ( int i = 0; i < 7; i++ ) {
		const float ry = tableY + (float)i * rowH;
		SCR_UIRowBegin( i );
		if ( i % 2 == 0 ) {
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, 0.05f };
			SCR_FillRect( winX + 12.0f, ry, winW - 24.0f, rowH - 2.0f, zebra );
		}
		const float ty = ry + ( rowH - 2.0f - 4.8f * 1.2f ) * 0.5f;
		SCR_DrawVirtualString( winX + 22.0f, ty, 4.6f, labels[i], dim );
		SCR_DrawVirtualString( winX + winW - 22.0f - SCR_GetStringWidth( vals[i], 4.8f ), ty, 4.8f, vals[i], white );
		SCR_UIRowEnd();
	}
	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Press ^3F8^7, ^3ESC^7 or type ^3!stats^7 to close" );
}

//=======================================================

/*
==================
SCR_DrawToastOverlay

Personal Victory / Defeat toast banner shown only to the duel participant.
Auto-dismisses after TOAST_DURATION_MS with fade-in / fade-out animation.
==================
*/
#define TOAST_DURATION_MS  6000
#define TOAST_FADEIN_MS    350
#define TOAST_FADEOUT_MS   500

void SCR_DrawToastOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgToast.active ) return;

	int elapsed = cls.realtime - g_rpgToast.startTimeMs;
	if ( elapsed >= TOAST_DURATION_MS ) {
		g_rpgToast.active = qfalse;
		return;
	}

	// alpha (fade-in / fade-out) and slide: comes in from the right edge and leaves the same way
	float alpha = 1.0f;
	if ( elapsed < TOAST_FADEIN_MS ) {
		alpha = (float)elapsed / (float)TOAST_FADEIN_MS;
	} else if ( elapsed > TOAST_DURATION_MS - TOAST_FADEOUT_MS ) {
		alpha = (float)(TOAST_DURATION_MS - elapsed) / (float)TOAST_FADEOUT_MS;
	}
	if ( alpha < 0.0f ) alpha = 0.0f;
	if ( alpha > 1.0f ) alpha = 1.0f;
	const float vis = SCR_UIEase( alpha );

	const float panelW = 190.0f;
	const float panelH = 56.0f;
	const float panelX = 640.0f - panelW - 14.0f + ( 1.0f - vis ) * ( panelW + 20.0f );
	const float panelY = 125.0f;                   // Under the map

	vec4_t accent = { 1.00f, 0.25f, 0.25f, alpha };
	if ( g_rpgToast.isWin ) { accent[0] = 0.20f; accent[1] = 1.00f; accent[2] = 0.40f; }
	vec4_t bg     = { 0.04f, 0.07f, 0.14f, 0.90f * alpha };
	vec4_t border = { accent[0] * 0.8f, accent[1] * 0.8f, accent[2] * 0.8f, 0.65f * alpha };
	vec4_t whiteA = { 1.0f, 1.0f, 1.0f, alpha };
	SCR_UIPanel( panelX, panelY, panelW, panelH, 8.0f, bg, border, accent );

	const float textX = panelX + 12.0f;
	SCR_DrawVirtualString( textX, panelY + 5.0f, 5.6f, g_rpgToast.isWin ? "^2VICTORY!" : "^1DEFEAT.", whiteA );
	SCR_DrawVirtualString( textX, panelY + 17.0f, 4.0f, va( "^7vs. ^5%.24s", g_rpgToast.opponentName ), whiteA );

	char row3[128];
	if ( g_rpgToast.isWin ) {
		Com_sprintf( row3, sizeof( row3 ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp );
	} else {
		Com_sprintf( row3, sizeof( row3 ), "^5ELO ^1%d ^7| ^6+%d CR", g_rpgToast.eloDelta, g_rpgToast.credits );
	}
	SCR_DrawVirtualString( textX, panelY + 28.0f, 3.8f, row3, whiteA );

	if ( g_rpgToast.health > 0 || g_rpgToast.bp > 0 ) {
		const char *hc = g_rpgToast.isWin ? "^2" : "^1";
		char row4[96];
		if ( g_rpgToast.bp > 0 ) Com_sprintf( row4, sizeof( row4 ), "%s%d HP ^7| ^5%d BP remaining", hc, g_rpgToast.health, g_rpgToast.bp );
		else Com_sprintf( row4, sizeof( row4 ), "%s%d HP ^7remaining", hc, g_rpgToast.health );
		SCR_DrawVirtualString( textX, panelY + 38.0f, 3.6f, row4, whiteA );
	}

	// timer bar (shrinks as the toast runs out)
	const float barW = panelW - 24.0f;
	const float barFill = barW * ( 1.0f - (float)elapsed / (float)TOAST_DURATION_MS );
	vec4_t barBg = { 0.10f, 0.10f, 0.18f, 0.70f * alpha };
	SCR_FillRect( panelX + 12.0f, panelY + panelH - 6.0f, barW, 2.0f, barBg );
	if ( barFill > 0.0f ) {
		SCR_FillRect( panelX + 12.0f, panelY + panelH - 6.0f, barFill, 2.0f, accent );
	}
}

/*
==================
SCR_DrawInspectOverlay

Crosshair hover card — polls server every 1.5s via "inspect <entityNum>".
Fades in when data is fresh and fades out after 2.5s of no data.
==================
*/
#define INSPECT_POLL_MS    1500
#define INSPECT_EXPIRE_MS  2500
#define INSPECT_FADEIN_MS  100
#define INSPECT_FADEOUT_MS 100

static int s_inspectLastPollMs = 0;
static int s_lastInspectedPlayer = -1;

void SCR_DrawInspectOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	// Check crosshair target every frame
	int crosshairNum = -1;
	if ( cls.cgameStarted ) {
		crosshairNum = CGVM_CrosshairPlayer();
	}

	// Poll server periodically if looking at a valid player (only on Ranked servers)
	if ( cl_isRankedServer && crosshairNum >= 0 && crosshairNum < 64 ) {
		if ( crosshairNum != s_lastInspectedPlayer || (cls.realtime - s_inspectLastPollMs >= 3000) ) {
			s_inspectLastPollMs = cls.realtime;
			s_lastInspectedPlayer = crosshairNum;
			CL_AddReliableCommand( va( "inspect %d", crosshairNum ), qfalse );
		}
	} else {
		s_lastInspectedPlayer = -1;
		// If we look away, shorten the expiration time immediately so it starts a fast fade-out
		if ( g_rpgInspect.active ) {
			int elapsed = cls.realtime - g_rpgInspect.lastUpdateMs;
			if ( elapsed < INSPECT_EXPIRE_MS - 100 ) {
				g_rpgInspect.lastUpdateMs = cls.realtime - (INSPECT_EXPIRE_MS - 100);
			}
		}
	}

	if ( !g_rpgInspect.active ) return;

	int age = cls.realtime - g_rpgInspect.lastUpdateMs;
	if ( age >= INSPECT_EXPIRE_MS ) {
		g_rpgInspect.active = qfalse;
		return;
	}

	// Alpha animation
	float alpha = 1.0f;
	if ( age < INSPECT_FADEIN_MS ) {
		alpha = (float)age / (float)INSPECT_FADEIN_MS;
	} else if ( age > INSPECT_EXPIRE_MS - INSPECT_FADEOUT_MS ) {
		alpha = (float)(INSPECT_EXPIRE_MS - age) / (float)INSPECT_FADEOUT_MS;
	}
	if ( alpha < 0.0f ) alpha = 0.0f;
	if ( alpha > 1.0f ) alpha = 1.0f;

	char statStr[64];
	Com_sprintf( statStr, sizeof( statStr ), "^5Lv %d  ^7|  ^2%d Elo", g_rpgInspect.level, g_rpgInspect.fr );

	float fontSize = 4.8f;
	float w = SCR_GetStringWidth( statStr, fontSize );
	float x = 320.0f - w * 0.5f - 3.0f;
	float y = 195.0f;                  // Directly under default crosshair name

	// Render see-through transparent text only (no UI box)
	vec4_t whiteA = { 1.0f, 1.0f, 1.0f, alpha * 0.70f };
	SCR_DrawVirtualString( x, y, fontSize, statStr, whiteA );
}



//=======================================================

/*
==================
SCR_DrawBountyOverlay

Modal popup sheet for !wanted (duel streaks) and !bountylist (online bounties)
==================
*/
void SCR_DrawBountyOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawBounty || !cg_drawBounty->integer ) return;
	if ( !g_rpgBounty.active ) return;

	const qboolean wanted = g_rpgBounty.isWanted;
	const int rows = ( g_rpgBounty.count > 10 ) ? 10 : g_rpgBounty.count;
	const float rowH = 19.0f;

	// keep the close-button area at the top, the window grows with the number of rows
	float winW = 300.0f;
	float winH = 92.0f + ( rows > 0 ? rows : 2 ) * rowH;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	vec4_t tint   = { 0.95f, 0.75f, 0.20f, 1.0f };
	if ( wanted ) { tint[0] = 1.00f; tint[1] = 0.30f; tint[2] = 0.25f; }
	vec4_t bg     = { 0.04f, 0.07f, 0.14f, 0.94f };
	vec4_t border = { tint[0] * 0.8f, tint[1] * 0.8f, tint[2] * 0.8f, 0.70f };
	vec4_t dim    = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white  = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIPanel( winX, winY, winW, winH, 10.0f, bg, border, tint );

	// title + close chip
	SCR_UITitle( winX + 18.0f, winY + 10.0f, winW - 36.0f, wanted ? "^1WANTED ^7TARGETS" : "^3ACTIVE ^7BOUNTIES", 7.0f, tint );
	vec4_t chipFill = { 0.30f, 0.06f, 0.06f, 0.9f };
	vec4_t chipBorder = { 0.90f, 0.30f, 0.25f, 0.9f };
	SCR_UIChip( winX + winW - 40.0f, winY + 8.0f, 34.0f, 15.0f, "ESC", 3.6f, chipFill, chipBorder, white );

	// column headers
	const float headY = winY + 42.0f;
	const float colName = winX + 40.0f;
	const float colStreak = winX + 180.0f;
	const float rightEdge = winX + winW - 18.0f;
	SCR_DrawVirtualString( winX + 20.0f, headY, 3.6f, "#", dim );
	SCR_DrawVirtualString( colName, headY, 3.6f, "PLAYER", dim );
	if ( wanted ) SCR_DrawVirtualString( colStreak, headY, 3.6f, "STREAK", dim );
	const char *bountyHead = wanted ? "BOUNTY" : "REWARD";
	SCR_DrawVirtualString( rightEdge - SCR_GetStringWidth( bountyHead, 3.6f ), headY, 3.6f, bountyHead, dim );

	if ( rows == 0 ) {
		SCR_DrawVirtualString( winX + 20.0f, headY + 22.0f, 4.6f, "^7No active targets found.", white );
	}

	float rowY = headY + 14.0f;
	for ( int i = 0; i < rows; i++ ) {
		bountyEntry_t *e = &g_rpgBounty.entries[i];

		if ( i % 2 == 0 ) {
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, 0.05f };
			SCR_FillRect( winX + 12.0f, rowY, winW - 24.0f, rowH - 1.0f, zebra );
		}

		// rank: gold / silver / bronze chips for the top three
		vec4_t chip = { 0.0f, 0.0f, 0.0f, 0.0f };
		vec4_t numCol = { 0.75f, 0.85f, 1.0f, 1.0f };
		if ( e->rank == 1 )      { chip[0] = 1.00f; chip[1] = 0.80f; chip[2] = 0.20f; chip[3] = 1.0f; }
		else if ( e->rank == 2 ) { chip[0] = 0.78f; chip[1] = 0.84f; chip[2] = 0.90f; chip[3] = 1.0f; }
		else if ( e->rank == 3 ) { chip[0] = 0.80f; chip[1] = 0.50f; chip[2] = 0.25f; chip[3] = 1.0f; }
		const float ty = rowY + ( rowH - 1.0f - 4.8f * 1.2f ) * 0.5f;
		if ( chip[3] > 0.0f ) {
			SCR_FillRect( winX + 18.0f, rowY + 2.0f, 14.0f, rowH - 5.0f, chip );
			vec4_t dark = { 0.05f, 0.05f, 0.08f, 1.0f };
			const char *rs = va( "%d", e->rank );
			SCR_DrawVirtualString( winX + 25.0f - SCR_GetStringWidth( rs, 4.6f ) * 0.5f, ty, 4.6f, rs, dark );
		} else {
			SCR_DrawVirtualString( winX + 22.0f, ty, 4.6f, va( "%d", e->rank ), numCol );
		}

		char nameFit[64];
		SCR_FitText( e->name, nameFit, sizeof( nameFit ), colStreak - colName - 8.0f, 4.8f );
		SCR_DrawVirtualString( colName, ty, 4.8f, va( "^7%s", nameFit ), white );

		if ( wanted ) {
			SCR_DrawVirtualString( colStreak, ty, 4.4f, va( "^5%d wins", e->streak ), white );
		}
		const char *bs = ( e->bounty > 0 ) ? va( "^3%d CR", e->bounty ) : "^7-";
		float bw = SCR_GetStringWidth( bs, 4.8f );
		SCR_DrawVirtualString( rightEdge - bw, ty, 4.8f, bs, white );

		rowY += rowH;
	}

	// footer
	vec4_t line = { tint[0], tint[1], tint[2], 0.35f };
	SCR_FillRect( winX + 18.0f, winY + winH - 22.0f, winW - 36.0f, 1.0f, line );
	const char *closeStr = wanted ? "^7Press ^3F8^7, ^3ESC^7 or type ^3!wanted^7 to close" : "^7Press ^3F8^7, ^3ESC^7 or type ^3!bountylist^7 to close";
	SCR_DrawVirtualString( winX + 18.0f, winY + winH - 16.0f, 3.8f, closeStr, white );
}


/*
==================
SCR_DrawShopOverlay

Interactive Glassmorphic Shop UI with long item description support
==================
*/
void SCR_DrawShopOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawShop || !cg_drawShop->integer ) return;
	if ( !g_rpgShop.active ) return;

	// geometry matches the click handling in cl_keys.cpp (rows from winY+62, 34 apart; BUY / SELL buttons)
	const float winW = 300.0f, winH = 400.0f;
	const float winX = 640.0f - winW - 14.0f;
	const float winY = 40.0f;
	vec4_t tint  = { 0.95f, 0.75f, 0.20f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t line  = { tint[0], tint[1], tint[2], 0.35f };
	SCR_UIWindow( winX, winY, winW, winH, "^3RANKED ^7SHOP", tint, qtrue );

	SCR_DrawVirtualString( winX + 18.0f, winY + 31.0f, 4.8f, va( "^7Credits: ^5%d CR", g_rpgShop.credits ), white );
	SCR_FillRect( winX + 12.0f, winY + 48.0f, winW - 24.0f, 1.0f, line );

	const float mx = (float)g_rpgMouseX;
	const float my = (float)g_rpgMouseY;

	if ( g_rpgShop.count == 0 ) {
		SCR_DrawVirtualString( winX + 18.0f, winY + 70.0f, 5.0f, "^7No items.", white );
	} else {
		const int maxVisible = 8;
		int startIdx = g_rpgShop.scroll;
		if ( startIdx < 0 ) startIdx = 0;

		float rowY = winY + 62.0f;
		for ( int i = startIdx; i < g_rpgShop.count && ( i - startIdx ) < maxVisible; i++ ) {
			rpgShopItem_t *item = &g_rpgShop.items[i];
			SCR_UIRowBegin( i - startIdx );

			const qboolean rowHover = (qboolean)( mx >= winX + 8.0f && mx <= winX + winW - 8.0f && my >= rowY && my <= rowY + 32.0f );
			vec4_t rowBg = { 1.0f, 1.0f, 1.0f, ( ( i - startIdx ) % 2 == 0 ) ? 0.05f : 0.02f };
			if ( rowHover ) { rowBg[0] = 0.20f; rowBg[1] = 0.70f; rowBg[2] = 1.00f; rowBg[3] = 0.16f; }
			SCR_FillRect( winX + 12.0f, rowY, winW - 24.0f, 32.0f, rowBg );
			if ( rowHover ) SCR_FillRect( winX + 12.0f, rowY, 2.0f, 32.0f, tint );

			SCR_DrawVirtualString( winX + 20.0f, rowY + 3.0f, 4.4f, va( "%s", item->display ), white );
			SCR_DrawVirtualString( winX + 20.0f, rowY + 17.0f, 3.8f, va( "^5Buy %d cr  ^7|  ^3Sell %d", item->price, item->sellBack ), white );

			const float btnBuyX = winX + winW - 95.0f;
			const float btnSellX = winX + winW - 55.0f;
			const float btnY = rowY + 6.0f;
			const float btnW = 35.0f, btnH = 18.0f;
			const qboolean buyHover = (qboolean)( mx >= btnBuyX && mx <= btnBuyX + btnW && my >= btnY && my <= btnY + btnH );
			const qboolean sellHover = (qboolean)( mx >= btnSellX && mx <= btnSellX + btnW && my >= btnY && my <= btnY + btnH );
			vec4_t green = { 0.20f, 0.85f, 0.40f, 1.0f };
			vec4_t red   = { 0.95f, 0.30f, 0.25f, 1.0f };
			SCR_UIButton( btnBuyX, btnY, btnW, btnH, "BUY", 3.8f, buyHover, green );
			SCR_UIButton( btnSellX, btnY, btnW, btnH, "SELL", 3.8f, sellHover, red );

			SCR_UIRowEnd();
			rowY += 34.0f;
		}

		if ( g_rpgShop.count > maxVisible ) {
			const float barX = winX + winW - 7.0f;
			const float barY = winY + 54.0f;
			const float barH = maxVisible * 34.0f - 4.0f;
			vec4_t scrollBg = { 0.05f, 0.10f, 0.20f, 0.60f };
			vec4_t scrollThumb = { 0.10f, 0.70f, 1.00f, 0.85f };
			SCR_FillRect( barX, barY, 3.0f, barH, scrollBg );
			const float calcH = barH * ( (float)maxVisible / (float)g_rpgShop.count );
			const float thumbH = ( calcH < 12.0f ) ? 12.0f : calcH;
			const float maxScroll = (float)( g_rpgShop.count - maxVisible );
			const float thumbY = barY + ( barH - thumbH ) * ( (float)startIdx / maxScroll );
			SCR_FillRect( barX, thumbY, 3.0f, thumbH, scrollThumb );
		}
	}

	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Scroll with the ^3mouse wheel^7, click BUY / SELL" );
}

/*
==================
SCR_DrawQuestInvOverlay

Combined Tabbed Modal for !quests and !inventory
==================
*/
/*
==================
SCR_DrawQuestOverlay

Displays the Daily Quests list on the quest_bg.tga outline frame
==================
*/
void SCR_DrawQuestOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawQuest || !cg_drawQuest->integer ) return;
	if ( !g_rpgQuest.active ) return;

	const float winW = 380.0f, winH = 304.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 88.0f;
	vec4_t tint  = { 0.25f, 0.85f, 0.55f, 1.0f };
	vec4_t dim   = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^2DAILY ^7QUESTS", tint, qtrue );

	if ( g_rpgQuest.questCount == 0 ) {
		SCR_DrawVirtualString( winX + 20.0f, winY + 50.0f, 5.0f, "^7No daily quests available.", white );
	} else {
		for ( int i = 0; i < g_rpgQuest.questCount && i < 3; i++ ) {
			rpgQuestEntry_t *q = &g_rpgQuest.quests[i];
			const float cx = winX + 16.0f, cw = winW - 32.0f;
			const float cy = winY + 38.0f + (float)i * 80.0f;
			SCR_UIRowBegin( i * 2 );

			vec4_t cardBg = { 0.06f, 0.12f, 0.20f, 0.85f };
			vec4_t cardBorder = { 0.20f, 0.45f, 0.65f, 0.60f };
			if ( q->done ) {
				cardBg[0] = 0.06f; cardBg[1] = 0.22f; cardBg[2] = 0.14f;
				cardBorder[0] = 0.25f; cardBorder[1] = 0.85f; cardBorder[2] = 0.45f; cardBorder[3] = 0.80f;
			}
			SCR_UIPanel( cx, cy, cw, 72.0f, 7.0f, cardBg, cardBorder, NULL );

			char descFit[96];
			SCR_FitText( q->desc, descFit, sizeof( descFit ), cw - 80.0f, 5.2f );
			SCR_DrawVirtualString( cx + 14.0f, cy + 8.0f, 5.2f, va( "^3%d. ^7%s", q->id, descFit ), white );
			if ( q->done ) {
				vec4_t chipFill = { 0.08f, 0.30f, 0.16f, 1.0f };
				vec4_t chipBorder = { 0.25f, 0.85f, 0.45f, 1.0f };
				SCR_UIChip( cx + cw - 50.0f, cy + 6.0f, 38.0f, 14.0f, "DONE", 3.8f, chipFill, chipBorder, white );
			}

			// progress bar fills in as the window opens
			float ratio = q->goal > 0 ? (float)q->prog / (float)q->goal : 0.0f;
			if ( q->done ) ratio = 1.0f;
			vec4_t lo = { 0.08f, 0.55f, 0.90f, 1.0f };
			vec4_t hi = { 0.30f, 0.85f, 1.00f, 1.0f };
			if ( q->done ) { lo[0] = 0.12f; lo[1] = 0.60f; lo[2] = 0.28f; hi[0] = 0.35f; hi[1] = 0.90f; hi[2] = 0.50f; }
			SCR_UIBar( cx + 14.0f, cy + 30.0f, cw - 90.0f, 7.0f, ratio * SCR_UIOpenEase( 250.0f + 120.0f * (float)i, 600.0f ), lo, hi );
			const char *progStr = q->done ? "^2Completed" : va( "^7%d / %d", q->prog, q->goal );
			SCR_DrawVirtualString( cx + cw - 70.0f, cy + 29.0f, 4.2f, progStr, white );

			SCR_DrawVirtualString( cx + 14.0f, cy + 48.0f, 4.4f, va( "^5Rewards: ^3+%d CR  ^2+%d Elo", q->rewardCr, q->rewardFr ), white );
			const char *modeStr = va( "^6%s", q->mode );
			SCR_DrawVirtualString( cx + cw - 14.0f - SCR_GetStringWidth( modeStr, 4.4f ), cy + 48.0f, 4.4f, modeStr, white );
			SCR_UIRowEnd();
		}
	}
	(void)dim;
	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Complete daily quests to earn rewards  |  ^3ESC^7 to close" );
}

/*
==================
SCR_DrawInventoryOverlay

Displays a sleek right-aligned inventory HUD panel with interactive USE buttons
==================
*/
void SCR_DrawInventoryOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawInventory || !cg_drawInventory->integer ) return;
	if ( !g_rpgInventory.active ) return;

	// geometry matches the click handling in cl_keys.cpp (rows from winY+68, 36 apart; USE button)
	const float winW = 240.0f, winH = 350.0f;
	const float winX = 640.0f - winW - 14.0f;
	const float winY = 50.0f;
	vec4_t tint  = { 0.20f, 0.70f, 1.00f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^5IN^7VENTORY", tint, qtrue );
	SCR_DrawVirtualString( winX + 18.0f, winY + 36.0f, 4.0f, va( "^7%d item type%s", g_rpgInventory.invCount, g_rpgInventory.invCount == 1 ? "" : "s" ), white );

	const float mx = (float)g_rpgMouseX;
	const float my = (float)g_rpgMouseY;

	if ( g_rpgInventory.invCount == 0 ) {
		SCR_DrawVirtualString( winX + 18.0f, winY + 80.0f, 5.0f, "^7Inventory empty.", white );
	} else {
		const int maxVisible = 6;
		int startIdx = g_rpgInventory.scroll;
		if ( startIdx < 0 ) startIdx = 0;

		float rowY = winY + 68.0f;
		for ( int i = startIdx; i < g_rpgInventory.invCount && ( i - startIdx ) < maxVisible; i++ ) {
			rpgInvEntry_t *item = &g_rpgInventory.inv[i];
			SCR_UIRowBegin( i - startIdx );

			char cleanDisplay[256];
			Q_strncpyz( cleanDisplay, item->display, sizeof( cleanDisplay ) );
			if ( strstr( cleanDisplay, "5DB7A2" ) || strstr( cleanDisplay, "XP Boost" ) ) {
				Com_sprintf( cleanDisplay, sizeof( cleanDisplay ), "^2XP Boost ^7(50+ for 1 round)" );
			}

			const qboolean rowHover = (qboolean)( mx >= winX + 8.0f && mx <= winX + winW - 8.0f && my >= rowY && my <= rowY + 34.0f );
			vec4_t rowBg = { 1.0f, 1.0f, 1.0f, ( ( i - startIdx ) % 2 == 0 ) ? 0.05f : 0.02f };
			if ( rowHover ) { rowBg[0] = 0.20f; rowBg[1] = 0.70f; rowBg[2] = 1.00f; rowBg[3] = 0.16f; }
			SCR_FillRect( winX + 12.0f, rowY, winW - 24.0f, 34.0f, rowBg );
			if ( rowHover ) SCR_FillRect( winX + 12.0f, rowY, 2.0f, 34.0f, tint );

			char fit[256];
			SCR_FitText( cleanDisplay, fit, sizeof( fit ), winW - 110.0f, 4.6f );
			SCR_DrawVirtualString( winX + 20.0f, rowY + 4.0f, 4.6f, fit, white );
			SCR_DrawVirtualString( winX + 20.0f, rowY + 18.0f, 4.0f, va( "^5Owned: x%d", item->qty ), white );

			const float btnUseX = winX + winW - 71.0f;
			const float btnY = rowY + 8.0f;
			const float btnW = 45.0f, btnH = 18.0f;
			const qboolean useHover = (qboolean)( mx >= btnUseX && mx <= btnUseX + btnW && my >= btnY && my <= btnY + btnH );
			vec4_t green = { 0.20f, 0.85f, 0.40f, 1.0f };
			SCR_UIButton( btnUseX, btnY, btnW, btnH, "USE", 3.8f, useHover, green );

			SCR_UIRowEnd();
			rowY += 36.0f;
		}

		if ( g_rpgInventory.invCount > maxVisible ) {
			const float barX = winX + winW - 7.0f;
			const float barY = winY + 62.0f;
			const float barH = maxVisible * 36.0f - 4.0f;
			vec4_t scrollBg = { 0.05f, 0.10f, 0.20f, 0.60f };
			vec4_t scrollThumb = { 0.10f, 0.70f, 1.00f, 0.85f };
			SCR_FillRect( barX, barY, 3.0f, barH, scrollBg );
			const float calcH = barH * ( (float)maxVisible / (float)g_rpgInventory.invCount );
			const float thumbH = ( calcH < 12.0f ) ? 12.0f : calcH;
			const float maxScroll = (float)( g_rpgInventory.invCount - maxVisible );
			const float thumbY = barY + ( barH - thumbH ) * ( (float)startIdx / maxScroll );
			SCR_FillRect( barX, thumbY, 3.0f, thumbH, scrollThumb );
		}
	}

	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Scroll with the ^3mouse wheel^7, click USE" );
}

/*
==================
SCR_DrawAchievementsOverlay

Achievements Modal List
==================
*/
void SCR_DrawAchievementsOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawAch || !cg_drawAch->integer ) return;
	if ( !g_rpgAch.active ) return;

	const float winW = 270.0f, winH = 360.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 60.0f;
	vec4_t tint  = { 0.95f, 0.75f, 0.20f, 1.0f };
	vec4_t dim   = { 0.50f, 0.58f, 0.68f, 1.0f };
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^3ACHIEVEMENTS", tint, qtrue );

	int unlocked = 0;
	for ( int i = 0; i < g_rpgAch.count && i < 9; i++ ) if ( g_rpgAch.entries[i].unlocked ) unlocked++;
	const int shown = g_rpgAch.count > 9 ? 9 : g_rpgAch.count;

	if ( g_rpgAch.count == 0 ) {
		SCR_DrawVirtualString( winX + 20.0f, winY + 50.0f, 5.0f, "^7No achievements loaded.", white );
	} else {
		const float rowH = 28.0f;
		const float rowStart = winY + 40.0f;
		for ( int i = 0; i < shown; i++ ) {
			rpgAchEntry_t *e = &g_rpgAch.entries[i];
			const float ry = rowStart + (float)i * rowH;
			SCR_UIRowBegin( i );
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, ( i % 2 == 0 ) ? 0.05f : 0.02f };
			SCR_FillRect( winX + 12.0f, ry, winW - 24.0f, rowH - 2.0f, zebra );

			const float ty = ry + ( rowH - 2.0f - 4.4f * 1.2f ) * 0.5f;
			// status box: filled green for unlocked, hollow grey for locked
			const float bx = winX + 20.0f, by = ry + ( rowH - 2.0f - 9.0f ) * 0.5f;
			if ( e->unlocked ) {
				vec4_t g = { 0.25f, 0.85f, 0.45f, 1.0f };
				SCR_FillRect( bx, by, 9.0f, 9.0f, g );
				vec4_t dark = { 0.04f, 0.10f, 0.06f, 1.0f };
				SCR_FillRect( bx + 2.0f, by + 4.5f, 2.0f, 2.0f, dark );
				SCR_FillRect( bx + 4.0f, by + 2.5f, 2.0f, 4.0f, dark );
			} else {
				SCR_FillRect( bx, by, 9.0f, 1.0f, dim );
				SCR_FillRect( bx, by + 8.0f, 9.0f, 1.0f, dim );
				SCR_FillRect( bx, by, 1.0f, 9.0f, dim );
				SCR_FillRect( bx + 8.0f, by, 1.0f, 9.0f, dim );
			}
			char fit[96];
			SCR_FitText( e->name, fit, sizeof( fit ), winW - 130.0f, 4.4f );
			SCR_DrawVirtualString( bx + 16.0f, ty, 4.4f, fit, e->unlocked ? white : dim );
			const char *rw = va( "%s+%d CR", e->unlocked ? "^5" : "^3", e->rewardCr );
			SCR_DrawVirtualString( winX + winW - 20.0f - SCR_GetStringWidth( rw, 4.2f ), ty, 4.2f, rw, white );
			SCR_UIRowEnd();
		}
	}
	SCR_UIFooter( winX, winY, winW, winH, tint, va( "^7Unlocked ^3%d^7 / %d  |  ^1ESC^7 to close", unlocked, shown ) );
}

/*
==================
SCR_DrawTopCreditsOverlay

Top Credits Leaderboard Card (Unified Leaderboard Size and Layout)
==================
*/
void SCR_DrawTopCreditsOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawTopCredits || !cg_drawTopCredits->integer ) return;
	if ( !g_rpgTopCredits.active ) return;

	char names[10][64];
	char values[10][24];
	const int n = g_rpgTopCredits.count > 10 ? 10 : g_rpgTopCredits.count;
	for ( int i = 0; i < n; i++ ) {
		Q_strncpyz( names[i], g_rpgTopCredits.entries[i].name, sizeof( names[i] ) );
		Com_sprintf( values[i], sizeof( values[i] ), "^5%d CR", g_rpgTopCredits.entries[i].credits );
	}
	vec4_t tint = { 0.25f, 0.85f, 0.45f, 1.0f };
	SCR_UIDrawTopList( "^2TOP ^7WEALTH", tint, "CREDITS", n, names, values, "^7Press ^1ESC^7 to close the Wealth Leaderboard" );
}


/*
==================
SCR_DrawTopPotatoOverlay

Top Hot Potato Leaderboard Card (Unified Leaderboard Size and Layout)
==================
*/
void SCR_DrawTopPotatoOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawTopPotato || !cg_drawTopPotato->integer ) return;
	if ( !g_rpgTopPotato.active ) return;

	char names[10][64];
	char values[10][24];
	const int n = g_rpgTopPotato.count > 10 ? 10 : g_rpgTopPotato.count;
	for ( int i = 0; i < n; i++ ) {
		Q_strncpyz( names[i], g_rpgTopPotato.entries[i].name, sizeof( names[i] ) );
		Com_sprintf( values[i], sizeof( values[i] ), "^7%d", g_rpgTopPotato.entries[i].ticks );
	}
	vec4_t tint = { 1.00f, 0.50f, 0.15f, 1.0f };
	SCR_UIDrawTopList( "^1TOP ^7POTATO", tint, "TICKS", n, names, values, "^7Press ^1ESC^7 to close the Potato Leaderboard" );
}


/*
==================
SCR_DrawAdventureOverlay

RPG Adventure Choice Card
==================
*/
void SCR_DrawAdventureOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawAdv || !cg_drawAdv->integer ) return;
	if ( !g_rpgAdv.active ) return;

	// same geometry as the click handling in cl_keys.cpp
	float winW = 412.0f;
	float winH = 220.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 130.0f;

	vec4_t tint   = { 0.70f, 0.35f, 0.95f, 1.0f };
	vec4_t bg     = { 0.04f, 0.07f, 0.14f, 0.94f };
	vec4_t border = { 0.55f, 0.30f, 0.80f, 0.75f };
	vec4_t dim    = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white  = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_UIPanel( winX, winY, winW, winH, 10.0f, bg, border, tint );

	// title + close chip
	char title[160];
	Com_sprintf( title, sizeof( title ), "^6ADVENTURE ^7| ^3%s", g_rpgAdv.title[0] ? g_rpgAdv.title : "Quest" );
	SCR_UITitle( winX + 18.0f, winY + 9.0f, winW - 36.0f - 40.0f, title, 5.6f, tint );
	vec4_t chipFill = { 0.30f, 0.06f, 0.06f, 0.9f };
	vec4_t chipBorder = { 0.90f, 0.30f, 0.25f, 0.9f };
	SCR_UIChip( winX + winW - 42.0f, winY + 8.0f, 36.0f, 15.0f, "ESC", 3.6f, chipFill, chipBorder, white );

	// story text (word-wrapped inside the panel)
	float startY = winY + 36.0f;
	g_rpgAdv.choiceY = SCR_DrawVirtualStringWrapped( winX + 18.0f, startY, 4.4f, winW - 36.0f, g_rpgAdv.text, white ) + 8.0f;
	float choiceY = g_rpgAdv.choiceY;

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	const char *choices[3] = { g_rpgAdv.choice1, g_rpgAdv.choice2, g_rpgAdv.choice3 };
	for ( int i = 0; i < 3; i++ ) {
		if ( !choices[i][0] ) continue;
		const float rx = winX + 16.0f, rw = winW - 32.0f, ry = choiceY - 6.0f, rh = 16.0f;
		const qboolean hover = (qboolean)( mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh + 2.0f );

		vec4_t rowBg = { 1.0f, 1.0f, 1.0f, 0.05f };
		vec4_t bar = { tint[0], tint[1], tint[2], 0.8f };
		if ( hover ) {
			rowBg[0] = 0.20f; rowBg[1] = 0.65f; rowBg[2] = 1.00f; rowBg[3] = 0.30f;
			bar[0] = 0.30f; bar[1] = 0.85f; bar[2] = 1.00f; bar[3] = 1.0f;
		}
		SCR_FillRect( rx, ry, rw, rh, rowBg );
		SCR_FillRect( rx, ry, 2.0f, rh, bar );

		// number chip, then the choice text
		vec4_t chipFill = { 0.06f, 0.10f, 0.20f, 0.95f };
		SCR_UIChip( rx + 6.0f, ry + 2.0f, 12.0f, rh - 4.0f, va( "%d", i + 1 ), 4.0f, chipFill, bar, white );
		SCR_DrawVirtualString( rx + 24.0f, choiceY, 5.0f, va( "%s%s", hover ? "^5" : "^7", choices[i] ), white );
		choiceY += 18.0f;
	}

	vec4_t line = { tint[0], tint[1], tint[2], 0.35f };
	SCR_FillRect( winX + 18.0f, winY + winH - 22.0f, winW - 36.0f, 1.0f, line );
	const char *hint = g_rpgAdv.choice1[0] ? "^7Click or press ^31^7, ^32^7, ^33^7 to choose  |  ^1!adv^7 or ^1ESC^7 to close"
										   : "^7Press ^3Space^7, ^3Enter^7 or ^3ESC^7 to close";
	SCR_DrawVirtualString( winX + 18.0f, winY + winH - 16.0f, 3.8f, hint, white );
}

static const char *s_avatarPaths[] = {
	"gfx/rpg_hud/avatar_default",
	"models/players/jedi/icon_default",
	"models/players/luke/icon_default",
	"models/players/kyle/icon_default",
	"models/players/reborn/icon_default"
};
#define NUM_AVATARS 5

int SCR_GetCurrentAvatarIndex( void ) {
	for ( int i = 0; i < NUM_AVATARS; i++ ) {
		if ( !Q_stricmp( cg_rpg_avatar->string, s_avatarPaths[i] ) ) {
			return i;
		}
	}
	return 0;
}

void SCR_SetAvatarIndex( int idx ) {
	if ( idx < 0 || idx >= NUM_AVATARS ) idx = 0;
	Cvar_Set( "cg_rpg_avatar", s_avatarPaths[idx] );
}

static void SCR_DrawSettingButton( float x, float y, float w, float h, const char *text, float textSz, qboolean hover ) {
	vec4_t tint = { 0.20f, 0.70f, 1.00f, 1.0f };
	SCR_UIButton( x, y, w, h, text, textSz, hover, tint );
}

static void SCR_DrawSettingSlider( float x, float y, float w, float h, float progress, const char *label, const char *valueText ) {
	SCR_UISlider( x, y, w, h, progress, label, valueText );
}

static qboolean s_draggingSliderX = qfalse;
static qboolean s_draggingSliderY = qfalse;
static qboolean s_draggingPartySliderX = qfalse;
static qboolean s_draggingPartySliderY = qfalse;

void SCR_DrawSettingsOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgSettings.active ) return;

	float winW = 480.0f;
	float winH = 360.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 240.0f - winH * 0.5f;

	if ( s_hSettingsBg <= 0 && re && re->RegisterShader ) {
		s_hSettingsBg = re->RegisterShader( "gfx/rpg_hud/hud_settings_bg" );
		s_hSettingsBtnNormal = re->RegisterShader( "gfx/rpg_hud/hud_btn_normal" );
		s_hSettingsBtnHover = re->RegisterShader( "gfx/rpg_hud/hud_btn_hover" );
		s_hSettingsSliderTrack = re->RegisterShader( "gfx/rpg_hud/hud_slider_track" );
		s_hSettingsSliderThumb = re->RegisterShader( "gfx/rpg_hud/hud_slider_thumb" );
	}

	if ( s_hSettingsBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hSettingsBg );
	} else {
		vec4_t bgColor = { 0.02f, 0.04f, 0.08f, 0.95f };
		vec4_t borderColor = { 0.0f, 0.8f, 1.0f, 0.8f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 8.0f, bgColor, borderColor );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	float headerSz = 5.2f;
	float headerW = SCR_GetStringWidth( "HUD CONFIGURATION STUDIO", headerSz );
	SCR_DrawVirtualString( winX + (winW - headerW) * 0.5f, winY + 28.0f, headerSz, "HUD CONFIGURATION STUDIO", yellowCol );

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	float col1X = winX + 40.0f;
	float col2X = winX + 240.0f;

	if ( kg.keys[A_MOUSE1].down ) {
		if ( !s_draggingSliderX && !s_draggingSliderY ) {
			if ( mx >= col2X && mx <= col2X + 200.0f && my >= winY + 80.0f && my <= winY + 110.0f ) {
				s_draggingSliderX = qtrue;
			}
			else if ( mx >= col2X && mx <= col2X + 200.0f && my >= winY + 140.0f && my <= winY + 170.0f ) {
				s_draggingSliderY = qtrue;
			}
		}

		if ( s_draggingSliderX ) {
			float newProgress = (mx - col2X) / 200.0f;
			if ( newProgress < 0.0f ) newProgress = 0.0f;
			if ( newProgress > 1.0f ) newProgress = 1.0f;
			float newVal = newProgress * 640.0f;
			Cvar_SetValue( "cg_rpg_x", newVal );
		}
		if ( s_draggingSliderY ) {
			float newProgress = (mx - col2X) / 200.0f;
			if ( newProgress < 0.0f ) newProgress = 0.0f;
			if ( newProgress > 1.0f ) newProgress = 1.0f;
			float newVal = newProgress * 480.0f;
			Cvar_SetValue( "cg_rpg_y", newVal );
		}
	} else {
		s_draggingSliderX = qfalse;
		s_draggingSliderY = qfalse;
	}

	int curStyle = cg_rpg_style ? cg_rpg_style->integer : 0;
	const char *styleLabel = ( curStyle == 1 ) ? "STYLE: EDGE" : "STYLE: HOLO";

	qboolean styleHover = (qboolean)( mx >= col1X && mx <= col1X + 160.0f && my >= winY + 90.0f && my <= winY + 130.0f );
	SCR_DrawSettingButton( col1X, winY + 90.0f, 160.0f, 40.0f, styleLabel, 4.0f, styleHover );

	const char *curPos = (cg_rpg_pos && cg_rpg_pos->string[0]) ? cg_rpg_pos->string : "left";
	char posLabel[64];
	Com_sprintf( posLabel, sizeof( posLabel ), "POS: %s", curPos );

	qboolean posHover = (qboolean)( mx >= col1X && mx <= col1X + 160.0f && my >= winY + 150.0f && my <= winY + 190.0f );
	SCR_DrawSettingButton( col1X, winY + 150.0f, 160.0f, 40.0f, posLabel, 4.0f, posHover );

	qboolean resetHover = (qboolean)( mx >= col1X && mx <= col1X + 160.0f && my >= winY + 210.0f && my <= winY + 250.0f );
	SCR_DrawSettingButton( col1X, winY + 210.0f, 160.0f, 40.0f, "RESET DEFAULTS", 4.0f, resetHover );

	float curX = cg_rpg_x ? cg_rpg_x->value : 14.0f;
	if ( curX < 0.0f ) curX = 0.0f;
	if ( curX > 640.0f ) curX = 640.0f;
	float progressX = curX / 640.0f;
	char xText[32];
	Com_sprintf( xText, sizeof( xText ), "%d px", (int)curX );
	SCR_DrawSettingSlider( col2X, winY + 90.0f, 200.0f, 12.0f, progressX, "X COORDINATE", xText );

	float curY = cg_rpg_y ? cg_rpg_y->value : 14.0f;
	if ( curY < 0.0f ) curY = 0.0f;
	if ( curY > 480.0f ) curY = 480.0f;
	float progressY = curY / 480.0f;
	char yText[32];
	Com_sprintf( yText, sizeof( yText ), "%d px", (int)curY );
	SCR_DrawSettingSlider( col2X, winY + 150.0f, 200.0f, 12.0f, progressY, "Y COORDINATE", yText );

	vec4_t labelCol = { 0.8f, 0.8f, 0.8f, 1.0f };
	SCR_DrawVirtualString( col2X, winY + 195.0f, 4.0f, "PLAYER AVATAR", labelCol );

	qboolean avLeftHover = (qboolean)( mx >= col2X && mx <= col2X + 40.0f && my >= winY + 210.0f && my <= winY + 250.0f );
	SCR_DrawSettingButton( col2X, winY + 210.0f, 40.0f, 40.0f, "<", 4.5f, avLeftHover );

	int curAvIdx = SCR_GetCurrentAvatarIndex();
	qhandle_t hAvIcon = 0;
	if ( curAvIdx >= 0 && curAvIdx < NUM_AVATARS && re && re->RegisterShader ) {
		hAvIcon = re->RegisterShader( s_avatarPaths[curAvIdx] );
	}
	if ( hAvIcon > 0 ) {
		vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
		re->SetColor( white );
		SCR_DrawPic( col2X + 60.0f, winY + 210.0f, 40.0f, 40.0f, hAvIcon );
	}

	qboolean avRightHover = (qboolean)( mx >= col2X + 120.0f && mx <= col2X + 160.0f && my >= winY + 210.0f && my <= winY + 250.0f );
	SCR_DrawSettingButton( col2X + 120.0f, winY + 210.0f, 40.0f, 40.0f, ">", 4.5f, avRightHover );

	qboolean closeHover = (qboolean)( mx >= winX + (winW - 160.0f) * 0.5f && mx <= winX + (winW - 160.0f) * 0.5f + 160.0f && my >= winY + 290.0f && my <= winY + 330.0f );
	SCR_DrawSettingButton( winX + (winW - 160.0f) * 0.5f, winY + 290.0f, 160.0f, 40.0f, "CLOSE STUDIO", 4.0f, closeHover );
}



extern bool FX_WorldToScreen(vec3_t worldCoord, float *x, float *y);

/*
==================
SCR_DrawHotPotatoOverheadIcon

3D World Space Overhead Floating Potato Icon
==================
*/
void SCR_DrawHotPotatoOverheadIcon( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cls.cgameStarted ) return;
	if ( g_hotPotatoHolder < 0 ) return;
	if ( !cl.snap.valid ) return;

	vec3_t headPos;
	qboolean found = qfalse;

	// Check if local player holds the hot potato
	if ( g_hotPotatoHolder == cl.snap.ps.clientNum ) {
		VectorCopy( cl.snap.ps.origin, headPos );
		headPos[2] += 42.0f; // Static above head
		found = qtrue;
	} else {
		// Search active snapshot for holder's entity
		for ( int i = 0; i < cl.snap.numEntities; i++ ) {
			int parseIdx = ( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 );
			entityState_t *ent = &cl.parseEntities[parseIdx];
			if ( ent->number == g_hotPotatoHolder ) {
				VectorCopy( ent->pos.trBase, headPos );
				headPos[2] += 42.0f; // Static above head
				found = qtrue;
				break;
			}
		}
	}

	if ( found ) {
		float sx = 0.0f, sy = 0.0f;
		if ( FX_WorldToScreen( headPos, &sx, &sy ) ) {
			// Calculate perspective distance scaling so icon shrinks when far away
			vec3_t delta;
			VectorSubtract( headPos, cl.snap.ps.origin, delta );
			float dist = VectorLength( delta );
			float scale = 350.0f / ( dist + 200.0f );
			if ( scale > 1.1f ) scale = 1.1f;   // Max size when close
			if ( scale < 0.35f ) scale = 0.35f; // Min size when far away

			float iconSize = 32.0f * scale;

			if ( s_hPotatoPic <= 0 && re && re->RegisterShader ) {
				s_hPotatoPic = re->RegisterShader( "gfx/rpg_hud/potato" );
			}
			if ( s_hPotatoPic > 0 ) {
				SCR_DrawPic( sx - iconSize * 0.5f, sy - iconSize * 0.5f, iconSize, iconSize, s_hPotatoPic );
			}
		}
	}
}

/*
==================
SCR_DrawCollectionOverlay

Loot collection book (!collection): four set cards with their pieces and a strip for the legendary finds.
Data comes from the server (col_set / col_piece).
==================
*/
rpgCollectionOverlay_t g_rpgCollection = {qfalse, 0, {}};

static void SCR_ColRarityColor( int rarity, vec4_t out ) {
	static const vec4_t cols[4] = {
		{ 0.78f, 0.80f, 0.85f, 1.0f },
		{ 0.30f, 0.85f, 0.40f, 1.0f },
		{ 0.40f, 0.62f, 1.00f, 1.0f },
		{ 1.00f, 0.80f, 0.20f, 1.0f }
	};
	if ( rarity < 0 ) rarity = 0;
	if ( rarity > 3 ) rarity = 3;
	out[0] = cols[rarity][0]; out[1] = cols[rarity][1]; out[2] = cols[rarity][2]; out[3] = cols[rarity][3];
}

static void SCR_DrawColPieces( const rpgColSet_t *cs, float x, float y, float w, float rowH, float charSize ) {
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	for ( int p = 0; p < 4; p++ ) {
		const float py = y + p * rowH;
		vec4_t rc;
		if ( cs->piece[p].qty > 0 ) {
			SCR_ColRarityColor( cs->piece[p].rarity, rc );
		} else {
			rc[0] = 0.25f; rc[1] = 0.30f; rc[2] = 0.38f; rc[3] = 1.0f;
		}
		SCR_FillRect( x, py + 1.0f, 7.0f, 7.0f, rc );
		if ( cs->piece[p].qty > 0 ) {
			SCR_DrawVirtualString( x + 12.0f, py, charSize, cs->piece[p].name, white );
			if ( cs->piece[p].qty > 1 ) {
				SCR_DrawVirtualString( x + w - 26.0f, py, charSize, va( "^5x%d", cs->piece[p].qty ), white );
			}
		} else {
			vec4_t dim = { 0.45f, 0.52f, 0.60f, 1.0f };
			SCR_DrawVirtualString( x + 12.0f, py, charSize, "???", dim );
		}
	}
}

void SCR_DrawCollectionOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgCollection.active ) return;

	const float winW = 560.0f, winH = 372.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 240.0f - winH * 0.5f;

	vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.94f };
	vec4_t borderColor = { 0.10f, 0.75f, 0.95f, 0.90f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t cardBg = { 0.06f, 0.12f, 0.20f, 0.85f };
	vec4_t cardDone = { 0.06f, 0.22f, 0.14f, 0.85f };
	vec4_t cardBorder = { 0.20f, 0.45f, 0.65f, 0.60f };
	vec4_t doneBorder = { 0.25f, 0.85f, 0.45f, 0.80f };
	vec4_t segOff = { 0.14f, 0.22f, 0.32f, 1.0f };
	vec4_t segOn = { 0.10f, 0.75f, 0.95f, 1.0f };
	vec4_t segDone = { 0.25f, 0.85f, 0.45f, 1.0f };

	(void)bgColor; (void)borderColor; (void)yellowCol;
	vec4_t colTint = { 1.0f, 0.80f, 0.20f, 1.0f };
	SCR_UIWindow( winX, winY, winW, winH, "^3COLLECTION ^7BOOK", colTint, qtrue );

	int totalHave = 0, totalMax = 0;
	for ( int i = 0; i < 5; i++ ) {
		totalHave += g_rpgCollection.sets[i].have;
		totalMax += 4;
	}
	const char *foundStr = va( "^7Found ^3%d^7 / %d", totalHave, totalMax );
	SCR_DrawVirtualString( winX + winW - 52.0f - SCR_GetStringWidth( foundStr, 4.4f ), winY + 12.0f, 4.4f, foundStr, whiteColor );

	// Four set cards in a 2x2 grid
	const float gap = 8.0f;
	const float cardW = ( winW - 24.0f - gap ) * 0.5f;
	const float cardH = 118.0f;
	for ( int s = 0; s < 4; s++ ) {
		const rpgColSet_t *cs = &g_rpgCollection.sets[s];
		const float cx = winX + 12.0f + ( s % 2 ) * ( cardW + gap );
		const float cy = winY + 38.0f + ( s / 2 ) * ( cardH + gap );
		SCR_UIRowBegin( s );
		SCR_UIPanel( cx, cy, cardW, cardH, 7.0f, cs->done ? cardDone : cardBg, cs->done ? doneBorder : cardBorder, NULL );

		SCR_DrawVirtualString( cx + 10.0f, cy + 7.0f, 5.0f, va( "^3%s", cs->name[0] ? cs->name : "???" ), whiteColor );
		SCR_DrawVirtualString( cx + cardW - 38.0f, cy + 7.0f, 5.0f, va( "%s%d/4", cs->done ? "^2" : "^7", cs->have ), whiteColor );

		// four progress segments
		const float segW = ( cardW - 20.0f - 3 * 3.0f ) * 0.25f;
		for ( int k = 0; k < 4; k++ ) {
			SCR_FillRect( cx + 10.0f + k * ( segW + 3.0f ), cy + 24.0f, segW, 4.0f,
				k < cs->have ? ( cs->done ? segDone : segOn ) : segOff );
		}

		SCR_DrawColPieces( cs, cx + 10.0f, cy + 36.0f, cardW - 20.0f, 14.0f, 4.4f );

		if ( cs->done ) {
			SCR_DrawVirtualString( cx + 10.0f, cy + cardH - 16.0f, 4.2f, "^2COMPLETE", whiteColor );
		} else if ( cs->reward > 0 ) {
			SCR_DrawVirtualString( cx + 10.0f, cy + cardH - 16.0f, 4.2f, va( "^7Set bonus: ^3+%d CR", cs->reward ), whiteColor );
		}
		SCR_UIRowEnd();
	}

	// Legendary strip
	{
		const rpgColSet_t *cs = &g_rpgCollection.sets[4];
		const float ly = winY + 38.0f + 2 * ( cardH + gap );
		const float lw = winW - 24.0f;
		vec4_t legBorder = { 1.0f, 0.80f, 0.20f, 0.55f };
		SCR_UIRowBegin( 4 );
		SCR_UIPanel( winX + 12.0f, ly, lw, 54.0f, 7.0f, cardBg, legBorder, NULL );
		SCR_DrawVirtualString( winX + 22.0f, ly + 6.0f, 4.8f, va( "^3Legendary Finds  ^7%d/4", cs->have ), whiteColor );
		for ( int p = 0; p < 4; p++ ) {
			const float px = winX + 22.0f + p * ( ( lw - 20.0f ) * 0.25f );
			const float py = ly + 26.0f;
			vec4_t rc;
			if ( cs->piece[p].qty > 0 ) SCR_ColRarityColor( 3, rc );
			else { rc[0] = 0.25f; rc[1] = 0.30f; rc[2] = 0.38f; rc[3] = 1.0f; }
			SCR_FillRect( px, py + 1.0f, 7.0f, 7.0f, rc );
			if ( cs->piece[p].qty > 0 ) SCR_DrawVirtualString( px + 12.0f, py, 4.2f, cs->piece[p].name, whiteColor );
			else {
				vec4_t dim = { 0.45f, 0.52f, 0.60f, 1.0f };
				SCR_DrawVirtualString( px + 12.0f, py, 4.2f, "???", dim );
			}
		}
	}

	SCR_UIRowEnd();
	SCR_UIFooter( winX, winY, winW, winH, colTint, "^7Loot drops from kills in FA and Legends.  Sell with ^3!sell <key>^7.  ^3ESC^7 to close" );
}

/*
==================
colui_test

Fills the collection book with sample data so the layout can be checked without a server.
==================
*/
static void SCR_RPGUITest_f( void ) {
	g_rpgStats.active = qfalse;
	g_rpgStats.level = 4; g_rpgStats.xp = 3245; g_rpgStats.fr = 1191;
	g_rpgStats.kills = 50; g_rpgStats.deaths = 70; g_rpgStats.wins = 28; g_rpgStats.losses = 27;
	g_rpgStats.credits = 12450;
	Q_strncpyz( g_rpgStats.rankTitle, "^3Youngling", sizeof( g_rpgStats.rankTitle ) );
	Cvar_Set( "cg_rpg_name", Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "Je'daii Likima" );
	Cvar_Set( "cg_rpg_rank", "^3Youngling" );
	Cvar_Set( "cg_rpg_level", "4" );
	Cvar_Set( "cg_rpg_fr", "1191" );
	Cvar_Set( "cg_rpg_xp", "245" );
	Cvar_Set( "cg_rpg_xp_max", "1000" );
	Cvar_Set( "cg_drawRPGHUD", "1" );
	if ( Cmd_Argc() > 2 ) Cvar_Set( "cg_rpg_style", Cmd_Argv( 2 ) );
}

static void SCR_RPGXPTest_f( void ) {
	int add = Cmd_Argc() > 1 ? atoi( Cmd_Argv( 1 ) ) : 120;
	int perLevel = g_rpgXpPerLevel > 0 ? g_rpgXpPerLevel : 1000;
	g_rpgStats.xp += add;
	g_rpgStats.level = g_rpgStats.xp / perLevel + 1;
	Cvar_Set( "cg_rpg_level", va( "%d", g_rpgStats.level ) );
	Cvar_Set( "cg_rpg_xp", va( "%d", g_rpgStats.xp % perLevel ) );
	Cvar_Set( "cg_rpg_xp_max", va( "%d", perLevel ) );
}

static void SCR_UIScreenTest_f( void ) {
	const char *w = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : "";
	static const char *nm[10] = { "Je'daii Likima", "Defiant", "Rey_Trainer", "Sentinel", "Cozy_Snoozer", "Mecca Chameleon", "Padawan_Kid", "Rinnegan", "DarkLord99", "NoobSlayer" };
	if ( !Q_stricmp( w, "lb" ) ) {
		g_topLeaderboardCount = 10;
		for ( int i = 0; i < 10; i++ ) {
			g_topLeaderboard[i].rank = i + 1;
			g_topLeaderboard[i].fr = 2400 - i * 130;
			g_topLeaderboard[i].level = 30 - i * 2;
			Q_strncpyz( g_topLeaderboard[i].rankTitle, i < 3 ? "Jedi Master" : "Padawan", sizeof( g_topLeaderboard[i].rankTitle ) );
			Q_strncpyz( g_topLeaderboard[i].displayName, nm[i], sizeof( g_topLeaderboard[i].displayName ) );
		}
		Cvar_Set( "cg_drawLeaderboard", "1" );
	} else if ( !Q_stricmp( w, "stats" ) ) {
		g_rpgStats.active = qtrue;
		g_rpgStats.level = 4; g_rpgStats.xp = 3245; g_rpgStats.elo = 1191; g_rpgStats.fr = 1191;
		g_rpgStats.credits = 12450; g_rpgStats.wins = 28; g_rpgStats.losses = 27; g_rpgStats.kills = 50; g_rpgStats.deaths = 70;
		g_rpgStats.highestStreak = 6; g_rpgStats.triviaWins = 3; g_rpgStats.topRivalCount = 5;
		Q_strncpyz( g_rpgStats.name, "Je'daii Likima", sizeof( g_rpgStats.name ) );
		Q_strncpyz( g_rpgStats.rankTitle, "^3Youngling", sizeof( g_rpgStats.rankTitle ) );
		Q_strncpyz( g_rpgStats.favWeapon, "Lightsaber", sizeof( g_rpgStats.favWeapon ) );
		Q_strncpyz( g_rpgStats.topRivalName, "Defiant", sizeof( g_rpgStats.topRivalName ) );
		Cvar_Set( "cg_drawStats", "1" );
	} else if ( !Q_stricmp( w, "wanted" ) || !Q_stricmp( w, "bounties" ) ) {
		g_rpgBounty.active = qtrue;
		g_rpgBounty.isWanted = !Q_stricmp( w, "wanted" ) ? qtrue : qfalse;
		g_rpgBounty.count = 6;
		for ( int i = 0; i < 6; i++ ) {
			g_rpgBounty.entries[i].rank = i + 1;
			g_rpgBounty.entries[i].streak = 9 - i;
			g_rpgBounty.entries[i].bounty = 900 - i * 120;
			Q_strncpyz( g_rpgBounty.entries[i].name, nm[i], sizeof( g_rpgBounty.entries[i].name ) );
		}
		Cvar_Set( "cg_drawBounty", "1" );
	} else if ( !Q_stricmp( w, "shop" ) ) {
		g_rpgShop.active = qtrue; g_rpgShop.credits = 12450; g_rpgShop.count = 10; g_rpgShop.scroll = 0;
		for ( int i = 0; i < 10; i++ ) {
			Q_strncpyz( g_rpgShop.items[i].key, va( "item%d", i ), sizeof( g_rpgShop.items[i].key ) );
			Q_strncpyz( g_rpgShop.items[i].display, va( "^7Shop Item %d ^5(+%d Force)", i + 1, i * 5 ), sizeof( g_rpgShop.items[i].display ) );
			g_rpgShop.items[i].price = 250 + i * 150;
			g_rpgShop.items[i].sellBack = 100 + i * 60;
		}
		Cvar_Set( "cg_drawShop", "1" );
	} else if ( !Q_stricmp( w, "inv" ) ) {
		g_rpgInventory.active = qtrue; g_rpgInventory.invCount = 4; g_rpgInventory.scroll = 0;
		for ( int i = 0; i < 4; i++ ) {
			Q_strncpyz( g_rpgInventory.inv[i].key, va( "inv%d", i ), sizeof( g_rpgInventory.inv[i].key ) );
			Q_strncpyz( g_rpgInventory.inv[i].display, va( "^7Inventory Item %d", i + 1 ), sizeof( g_rpgInventory.inv[i].display ) );
			g_rpgInventory.inv[i].qty = 1 + i;
		}
		Cvar_Set( "cg_drawInventory", "1" );
	} else if ( !Q_stricmp( w, "quest" ) ) {
		g_rpgQuest.active = qtrue; g_rpgQuest.questCount = 3;
		for ( int i = 0; i < 3; i++ ) {
			g_rpgQuest.quests[i].id = i + 1;
			Q_strncpyz( g_rpgQuest.quests[i].desc, i == 0 ? "Win 5 ranked duels" : ( i == 1 ? "Get 20 kills in Full Authentic" : "Reach a 3 win streak" ), sizeof( g_rpgQuest.quests[i].desc ) );
			g_rpgQuest.quests[i].goal = i == 0 ? 5 : ( i == 1 ? 20 : 3 );
			g_rpgQuest.quests[i].prog = i == 2 ? 3 : ( i == 0 ? 2 : 11 );
			g_rpgQuest.quests[i].done = i == 2 ? qtrue : qfalse;
			g_rpgQuest.quests[i].rewardCr = 300 + i * 100;
			g_rpgQuest.quests[i].rewardFr = 10 + i * 5;
			Q_strncpyz( g_rpgQuest.quests[i].mode, i == 1 ? "FA" : "Duel", sizeof( g_rpgQuest.quests[i].mode ) );
		}
		Cvar_Set( "cg_drawQuest", "1" );
	} else if ( !Q_stricmp( w, "ach" ) ) {
		g_rpgAch.active = qtrue; g_rpgAch.count = 9;
		for ( int i = 0; i < 9; i++ ) {
			Q_strncpyz( g_rpgAch.entries[i].name, va( "Achievement number %d", i + 1 ), sizeof( g_rpgAch.entries[i].name ) );
			g_rpgAch.entries[i].rewardCr = 100 * ( i + 1 );
			g_rpgAch.entries[i].unlocked = ( i % 3 == 0 ) ? qtrue : qfalse;
		}
		Cvar_Set( "cg_drawAch", "1" );
	} else if ( !Q_stricmp( w, "credits" ) ) {
		g_rpgTopCredits.active = qtrue; g_rpgTopCredits.count = 10;
		for ( int i = 0; i < 10; i++ ) {
			g_rpgTopCredits.entries[i].rank = i + 1;
			g_rpgTopCredits.entries[i].credits = 90000 - i * 7000;
			Q_strncpyz( g_rpgTopCredits.entries[i].name, nm[i], sizeof( g_rpgTopCredits.entries[i].name ) );
		}
		Cvar_Set( "cg_drawTopCredits", "1" );
	} else if ( !Q_stricmp( w, "potato" ) ) {
		g_rpgTopPotato.active = qtrue; g_rpgTopPotato.count = 10;
		for ( int i = 0; i < 10; i++ ) {
			g_rpgTopPotato.entries[i].rank = i + 1;
			g_rpgTopPotato.entries[i].ticks = 300 - i * 25;
			Q_strncpyz( g_rpgTopPotato.entries[i].name, nm[i], sizeof( g_rpgTopPotato.entries[i].name ) );
		}
		Cvar_Set( "cg_drawTopPotato", "1" );
	} else if ( !Q_stricmp( w, "adv" ) ) {
		g_rpgAdv.active = qtrue; g_rpgAdv.choiceY = 0.0f;
		Q_strncpyz( g_rpgAdv.title, "The Old Temple", sizeof( g_rpgAdv.title ) );
		Q_strncpyz( g_rpgAdv.text, "You find a crumbling temple half buried in the sand. A faint blue glow comes from inside.", sizeof( g_rpgAdv.text ) );
		Q_strncpyz( g_rpgAdv.choice1, "Step inside", sizeof( g_rpgAdv.choice1 ) );
		Q_strncpyz( g_rpgAdv.choice2, "Search the ruins outside", sizeof( g_rpgAdv.choice2 ) );
		Q_strncpyz( g_rpgAdv.choice3, "Leave quietly", sizeof( g_rpgAdv.choice3 ) );
		Cvar_Set( "cg_drawAdv", "1" );
	} else if ( !Q_stricmp( w, "menu" ) ) {
		g_rpgMenu.active = qtrue;
		g_rpgMenu.tab = Cmd_Argc() > 2 ? atoi( Cmd_Argv( 2 ) ) % 4 : 0;
	} else if ( !Q_stricmp( w, "toast" ) ) {
		g_rpgToast.active = qtrue; g_rpgToast.isWin = qtrue; g_rpgToast.eloDelta = 18; g_rpgToast.credits = 250; g_rpgToast.xp = 120;
		g_rpgToast.health = 64; g_rpgToast.bp = 0; g_rpgToast.startTimeMs = cls.realtime;
		Q_strncpyz( g_rpgToast.opponentName, "Defiant", sizeof( g_rpgToast.opponentName ) );
	} else {
		Com_Printf( "uiscreen_test <lb | stats | wanted | bounties | shop | inv | quest | ach | credits | potato | adv | menu [tab] | toast>\n" );
	}
}

static void SCR_ColUITest_f( void ) {
	memset( &g_rpgCollection.sets, 0, sizeof( g_rpgCollection.sets ) );
	static const char *setNames[5] = { "Rebel Alliance Kit", "Imperial Cache", "Jedi Order Relics", "Sith Artifacts", "Legendary Finds" };
	static const char *pieceNames[5][4] = {
		{ "^7Rebel Comlink", "^7Field Ration", "^2Pilot Helmet", "^5Hero's Medal" },
		{ "^7Access Keycard", "^7Scrap Plate", "^2Officer Cap", "^5Code Cylinder" },
		{ "^7Holorecord", "^7Training Remote", "^2Kyber Shard", "^5Jedi Robe" },
		{ "^7Sith Talisman", "^7Tome Page", "^2Dark Amulet", "^5Reactor Core" },
		{ "^3Heart of the Force", "^3Legend 2", "^3Legend 3", "^3Legend 4" }
	};
	static const int rar[4] = { 0, 0, 1, 2 };
	static const int have[5][4] = { { 2, 1, 1, 1 }, { 1, 0, 1, 0 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 }, { 1, 0, 0, 0 } };
	for ( int s = 0; s < 5; s++ ) {
		rpgColSet_t *cs = &g_rpgCollection.sets[s];
		Q_strncpyz( cs->name, setNames[s], sizeof( cs->name ) );
		cs->reward = ( s < 2 ) ? 400 : ( s < 4 ? 450 : 0 );
		for ( int p = 0; p < 4; p++ ) {
			cs->piece[p].qty = have[s][p];
			cs->piece[p].rarity = ( s == 4 ) ? 3 : rar[p];
			Q_strncpyz( cs->piece[p].name, have[s][p] ? pieceNames[s][p] : "???", sizeof( cs->piece[p].name ) );
			if ( have[s][p] ) cs->have++;
		}
		cs->done = ( s == 0 );
	}
	g_rpgCollection.setCount = 5;
	g_rpgCollection.active = qtrue;
}

/*
==================
SCR_DrawRPGMenuOverlay

Master RPG Hub Menu (!rpgmenu / !menu / !settings)
==================
*/
void SCR_DrawRPGMenuOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgMenu.active ) return;

	// geometry matches the click handling in cl_keys.cpp
	const float winW = 540.0f, winH = 390.0f;
	const float winX = 320.0f - winW * 0.5f;
	const float winY = 240.0f - winH * 0.5f;

	vec4_t tint   = { 0.10f, 0.75f, 0.95f, 1.0f };
	vec4_t dim    = { 0.60f, 0.72f, 0.88f, 1.0f };
	vec4_t white  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t panelBg = { 0.06f, 0.12f, 0.20f, 0.85f };
	vec4_t panelBorder = { 0.20f, 0.45f, 0.65f, 0.60f };
	const int now = cls.realtime;

	SCR_UIAnimBegin( 12 );
	SCR_UIWindow( winX, winY, winW, winH, "^3MASTER ^7RPG HUB", tint, qtrue );

	// ---- tabs with a sliding indicator
	static int s_menuLastTab = -1;
	static int s_menuTabMs = 0;
	static int s_menuLastDrawMs = 0;
	static float s_menuIndX = -1.0f;
	const qboolean reopened = ( now - s_menuLastDrawMs > 250 ) ? qtrue : qfalse;
	const float dt = reopened ? 0.0f : (float)( now - s_menuLastDrawMs ) * 0.001f;
	s_menuLastDrawMs = now;
	if ( reopened || s_menuLastTab != g_rpgMenu.tab ) {
		if ( reopened ) s_menuIndX = -1.0f;
		s_menuTabMs = now;
		s_menuLastTab = g_rpgMenu.tab;
	}

	const char *tabs[4] = { "PROFILE", "SETTINGS", "RANKS", "COMMANDS" };
	const float tabW = 120.0f;
	const float tabY = winY + 38.0f;
	const float tabStartX = winX + ( winW - ( 4 * tabW + 3 * 8.0f ) ) * 0.5f;
	const float mx = (float)g_rpgMouseX;
	const float my = (float)g_rpgMouseY;

	const float targetX = tabStartX + (float)g_rpgMenu.tab * ( tabW + 8.0f );
	if ( s_menuIndX < 0.0f ) s_menuIndX = targetX;
	else s_menuIndX += ( targetX - s_menuIndX ) * ( 1.0f - expf( -dt * 16.0f ) );

	for ( int i = 0; i < 4; i++ ) {
		const float tx = tabStartX + (float)i * ( tabW + 8.0f );
		const qboolean isSel = (qboolean)( g_rpgMenu.tab == i );
		const qboolean isHover = (qboolean)( mx >= tx && mx <= tx + tabW && my >= tabY && my <= tabY + 22.0f );
		vec4_t tabBg = { 0.05f, 0.12f, 0.24f, isSel ? 0.95f : ( isHover ? 0.75f : 0.45f ) };
		SCR_FillRect( tx, tabY, tabW, 22.0f, tabBg );
		const char *label = va( "%s%d  ^7%s", isSel ? "^3" : "^5", i + 1, tabs[i] );
		SCR_DrawVirtualString( tx + ( tabW - SCR_GetStringWidth( label, 4.4f ) ) * 0.5f, tabY + 5.5f, 4.4f, label, white );
	}
	vec4_t rule = { tint[0], tint[1], tint[2], 0.30f };
	SCR_FillRect( winX + 15.0f, tabY + 25.0f, winW - 30.0f, 1.0f, rule );
	SCR_FillRect( s_menuIndX, tabY + 22.5f, tabW, 2.5f, tint );

	const float contentY = tabY + 34.0f;
	const int tabAge = ( now - s_menuTabMs < s_uiAnimMs ) ? ( now - s_menuTabMs ) : s_uiAnimMs;

	// content fades and slides up a little whenever the tab changes
	{
		const float e = SCR_UIEase( (float)tabAge / 200.0f );
		s_uiBaseAlpha = s_uiAlpha * e;
		s_uiAlpha = s_uiBaseAlpha;
		s_uiBaseDY = s_uiDY + ( 1.0f - e ) * 6.0f;
		s_uiDY = s_uiBaseDY;
	}

	if ( g_rpgMenu.tab == 0 ) {
		// ---- PROFILE
		const float boxX = winX + 25.0f;
		const float boxW = winW - 50.0f;
		const int level = g_rpgStats.level > 0 ? g_rpgStats.level : ( cg_rpg_level ? cg_rpg_level->integer : 1 );
		const int fr = g_rpgStats.fr > 0 ? g_rpgStats.fr : ( cg_rpg_fr ? cg_rpg_fr->integer : 1000 );
		const char *name = g_rpgStats.name[0] ? g_rpgStats.name : ( ( cg_rpg_name && cg_rpg_name->string[0] ) ? cg_rpg_name->string : "Player" );
		const char *rank = g_rpgStats.rankTitle[0] ? g_rpgStats.rankTitle : ( ( cg_rpg_rank && cg_rpg_rank->string[0] ) ? cg_rpg_rank->string : "Padawan" );
		const int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;

		SCR_UIRowBeginMs( tabAge, 0 );
		SCR_UIPanel( boxX, contentY + 6.0f, boxW, 84.0f, 8.0f, panelBg, panelBorder, tint );
		const float avX = boxX + 16.0f, avY = contentY + 16.0f, avS = 64.0f;
		vec4_t avFrame = { 0.20f, 0.50f, 0.80f, 0.80f };
		vec4_t avBg = { 0.03f, 0.06f, 0.11f, 1.0f };
		SCR_FillRect( avX - 1.0f, avY - 1.0f, avS + 2.0f, avS + 2.0f, avFrame );
		SCR_FillRect( avX, avY, avS, avS, avBg );
		qhandle_t hAv = s_hAvatar;
		if ( !hAv && re ) hAv = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
		if ( hAv ) SCR_DrawPic( avX, avY, avS, avS, hAv );
		const float tx = avX + avS + 16.0f;
		char nameFit[96];
		SCR_FitText( name, nameFit, sizeof( nameFit ), boxW - avS - 40.0f - 90.0f, 6.4f );
		SCR_DrawVirtualString( tx, contentY + 14.0f, 6.4f, va( "^7%s", nameFit ), white );
		const char *lvStr = va( "^3Lv %d", level );
		SCR_DrawVirtualString( boxX + boxW - 16.0f - SCR_GetStringWidth( lvStr, 6.0f ), contentY + 14.0f, 6.0f, lvStr, white );
		SCR_DrawVirtualString( tx, contentY + 31.0f, 5.0f, va( "^3%s", rank ), white );
		SCR_DrawVirtualString( tx, contentY + 46.0f, 4.8f, va( "^5%d Elo   ^7|   ^5%d CR", fr, credits ), white );
		SCR_DrawVirtualString( tx, contentY + 61.0f, 4.4f, va( "^2%d W ^7/ ^1%d L   ^7(^3%d Kills^7 / ^1%d Deaths^7)", g_rpgStats.wins, g_rpgStats.losses, g_rpgStats.kills, g_rpgStats.deaths ), white );
		SCR_UIRowEnd();

		// XP progress
		SCR_UIRowBeginMs( tabAge, 1 );
		int relativeXP = cg_rpg_xp ? cg_rpg_xp->integer : 0;
		int relativeXPMax = ( cg_rpg_xp_max && cg_rpg_xp_max->integer > 0 ) ? cg_rpg_xp_max->integer : 1000;
		if ( relativeXP < 0 ) relativeXP = 0;
		if ( relativeXP > relativeXPMax ) relativeXP = relativeXPMax;
		const float ratio = (float)relativeXP / (float)relativeXPMax;
		SCR_DrawVirtualString( boxX, contentY + 100.0f, 4.2f, "^7Level progress", white );
		const char *xpStr = va( "^7%d / %d XP  ^5(%d%%)", relativeXP, relativeXPMax, (int)( ratio * 100.0f ) );
		SCR_DrawVirtualString( boxX + boxW - SCR_GetStringWidth( xpStr, 4.2f ), contentY + 100.0f, 4.2f, xpStr, white );
		vec4_t lo = { 0.08f, 0.55f, 0.90f, 1.0f };
		vec4_t hi = { 0.30f, 0.85f, 1.00f, 1.0f };
		SCR_UIBar( boxX, contentY + 114.0f, boxW, 9.0f, ratio * SCR_UIEase( ( (float)tabAge - 150.0f ) / 600.0f ), lo, hi );
		SCR_UIRowEnd();

		// account credentials
		SCR_UIRowBeginMs( tabAge, 2 );
		vec4_t credBg = { 0.07f, 0.16f, 0.10f, 0.85f };
		vec4_t credBorder = { 0.20f, 0.80f, 0.40f, 0.60f };
		vec4_t green = { 0.25f, 0.85f, 0.45f, 1.0f };
		SCR_UIPanel( boxX, contentY + 140.0f, boxW, 68.0f, 7.0f, credBg, credBorder, green );
		SCR_DrawVirtualString( boxX + 14.0f, contentY + 147.0f, 4.0f, "^2ACCOUNT CREDENTIALS", white );
		const char *uName = g_rpgStats.loginUser[0] ? g_rpgStats.loginUser : "Guest";
		const char *uPass = g_rpgStats.loginPass[0] ? g_rpgStats.loginPass : "None";
		SCR_DrawVirtualString( boxX + 14.0f, contentY + 164.0f, 4.4f, va( "^7Username   ^3%s", uName ), white );
		SCR_DrawVirtualString( boxX + 14.0f, contentY + 181.0f, 4.4f, va( "^7Password   ^3%s", uPass ), white );
		SCR_UIRowEnd();

		// ranked toggle (same hit box as before); the ranked state pulses softly
		SCR_UIRowBeginMs( tabAge, 3 );
		extern int g_clientRankedEnabled;
		const float btnX = boxX, btnY = contentY + 224.0f, btnW = boxW, btnH = 24.0f;
		const qboolean btnHover = (qboolean)( mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH );
		const float pulse = 0.85f + 0.15f * sinf( (float)now * 0.004f );
		vec4_t rankedTint = { 0.20f * pulse, 1.00f * pulse, 0.40f * pulse, 1.0f };
		vec4_t casualTint = { 1.00f, 0.60f, 0.20f, 1.0f };
		SCR_UIButton( btnX, btnY, btnW, btnH, g_clientRankedEnabled ? "^2RANKED MATCHES: ENABLED   (click to switch)" : "^3CASUAL MODE, NO ELO   (click to switch)", 4.4f, btnHover, g_clientRankedEnabled ? rankedTint : casualTint );
		SCR_UIRowEnd();

	} else if ( g_rpgMenu.tab == 1 ) {
		// ---- SETTINGS (hit boxes unchanged)
		SCR_DrawVirtualString( winX + 30.0f, contentY - 2.0f, 4.4f, "^5HUD & CHAT SETTINGS", white );

		const int curStyle = cg_rpg_style ? cg_rpg_style->integer : 0;
		const char *styleNames[2] = { "Holo", "Edge" };
		const char *curPos = ( cg_rpg_pos && cg_rpg_pos->string[0] ) ? cg_rpg_pos->string : "left";
		vec4_t btnTint = { 0.20f, 0.70f, 1.00f, 1.0f };

		SCR_UIRowBeginMs( tabAge, 0 );
		SCR_UIButton( winX + 30.0f, contentY + 20.0f, 210.0f, 35.0f, va( "^7HUD Style: ^3%s", styleNames[curStyle % 2] ), 4.4f,
			(qboolean)( mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 20.0f && my <= contentY + 55.0f ), btnTint );
		SCR_UIButton( winX + 270.0f, contentY + 20.0f, 210.0f, 35.0f, va( "^7Position: ^3%s", curPos ), 4.4f,
			(qboolean)( mx >= winX + 270.0f && mx <= winX + 480.0f && my >= contentY + 20.0f && my <= contentY + 55.0f ), btnTint );
		SCR_UIRowEnd();

		static cvar_t *cg_partyX = NULL;
		static cvar_t *cg_partyY = NULL;
		if ( !cg_partyX ) cg_partyX = Cvar_Get( "cg_partyX", "15", CVAR_ARCHIVE );
		if ( !cg_partyY ) cg_partyY = Cvar_Get( "cg_partyY", "140", CVAR_ARCHIVE );

		// dragging
		const float col2X = winX + 270.0f;
		if ( kg.keys[A_MOUSE1].down ) {
			if ( !s_draggingSliderX && !s_draggingSliderY && !s_draggingPartySliderX && !s_draggingPartySliderY ) {
				if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 60.0f && my <= contentY + 80.0f ) s_draggingSliderX = qtrue;
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 95.0f && my <= contentY + 115.0f ) s_draggingSliderY = qtrue;
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 130.0f && my <= contentY + 150.0f ) s_draggingPartySliderX = qtrue;
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 165.0f && my <= contentY + 185.0f ) s_draggingPartySliderY = qtrue;
			}
			float np = ( mx - col2X ) / 200.0f;
			if ( np < 0.0f ) np = 0.0f;
			if ( np > 1.0f ) np = 1.0f;
			if ( s_draggingSliderX ) Cvar_SetValue( "cg_rpg_x", np * 640.0f );
			if ( s_draggingSliderY ) Cvar_SetValue( "cg_rpg_y", np * 480.0f );
			if ( s_draggingPartySliderX ) Cvar_SetValue( "cg_partyX", np * 640.0f );
			if ( s_draggingPartySliderY ) Cvar_SetValue( "cg_partyY", np * 480.0f );
		} else {
			s_draggingSliderX = qfalse;
			s_draggingSliderY = qfalse;
			s_draggingPartySliderX = qfalse;
			s_draggingPartySliderY = qfalse;
		}

		SCR_UIRowBeginMs( tabAge, 1 );
		float curX = cg_rpg_x ? cg_rpg_x->value : 14.0f;
		if ( curX < 0.0f ) curX = 0.0f;
		if ( curX > 640.0f ) curX = 640.0f;
		SCR_UISlider( col2X, contentY + 65.0f, 200.0f, 12.0f, curX / 640.0f, "HUD X", va( "%d px", (int)curX ) );
		float curY = cg_rpg_y ? cg_rpg_y->value : 14.0f;
		if ( curY < 0.0f ) curY = 0.0f;
		if ( curY > 480.0f ) curY = 480.0f;
		SCR_UISlider( col2X, contentY + 100.0f, 200.0f, 12.0f, curY / 480.0f, "HUD Y", va( "%d px", (int)curY ) );
		float curPX = cg_partyX ? cg_partyX->value : 15.0f;
		if ( curPX < 0.0f ) curPX = 0.0f;
		if ( curPX > 640.0f ) curPX = 640.0f;
		SCR_UISlider( col2X, contentY + 135.0f, 200.0f, 12.0f, curPX / 640.0f, "PARTY X", va( "%d px", (int)curPX ) );
		float curPY = cg_partyY ? cg_partyY->value : 140.0f;
		if ( curPY < 0.0f ) curPY = 0.0f;
		if ( curPY > 480.0f ) curPY = 480.0f;
		SCR_UISlider( col2X, contentY + 170.0f, 200.0f, 12.0f, curPY / 480.0f, "PARTY Y", va( "%d px", (int)curPY ) );
		SCR_UIRowEnd();

		// avatar picker
		SCR_UIRowBeginMs( tabAge, 2 );
		SCR_DrawVirtualString( winX + 30.0f, contentY + 117.0f, 4.2f, "^7Avatar", white );
		const float avBoxX = winX + 160.0f, avBoxY = contentY + 108.0f;
		vec4_t avFrame = { 0.20f, 0.50f, 0.80f, 0.80f };
		SCR_FillRect( avBoxX - 1.0f, avBoxY - 1.0f, 36.0f, 36.0f, avFrame );
		if ( s_hAvatar > 0 ) SCR_DrawPic( avBoxX, avBoxY, 34.0f, 34.0f, s_hAvatar );
		SCR_UIButton( avBoxX - 45.0f, avBoxY + 3.0f, 35.0f, 30.0f, "<", 5.0f,
			(qboolean)( mx >= avBoxX - 45.0f && mx <= avBoxX - 10.0f && my >= avBoxY + 3.0f && my <= avBoxY + 33.0f ), btnTint );
		SCR_UIButton( avBoxX + 45.0f, avBoxY + 3.0f, 35.0f, 30.0f, ">", 5.0f,
			(qboolean)( mx >= avBoxX + 45.0f && mx <= avBoxX + 80.0f && my >= avBoxY + 3.0f && my <= avBoxY + 33.0f ), btnTint );
		SCR_UIRowEnd();

		// chat + reset
		SCR_UIRowBeginMs( tabAge, 3 );
		cvar_t *cPosCv = Cvar_Get( "cg_chat_pos", "0", CVAR_ARCHIVE );
		cvar_t *cStyCv = Cvar_Get( "cg_chat_style", "0", CVAR_ARCHIVE );
		const char *cPosNames[3] = { "Bottom", "Top", "Classic" };
		const char *cStyNames[4] = { "Dark Glass", "Cyber Cyan", "Solid Black", "No BG" };
		SCR_UIButton( winX + 30.0f, contentY + 160.0f, 210.0f, 33.0f, va( "^7Chat Pos: ^3%s", cPosNames[cPosCv->integer % 3] ), 4.2f,
			(qboolean)( mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 160.0f && my <= contentY + 193.0f ), btnTint );
		SCR_UIButton( winX + 30.0f, contentY + 200.0f, 210.0f, 33.0f, va( "^7Chat Style: ^3%s", cStyNames[cStyCv->integer % 4] ), 4.2f,
			(qboolean)( mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 200.0f && my <= contentY + 233.0f ), btnTint );
		vec4_t redTint = { 0.95f, 0.30f, 0.25f, 1.0f };
		SCR_UIButton( winX + 270.0f, contentY + 200.0f, 200.0f, 33.0f, "^1RESET DEFAULTS", 4.2f,
			(qboolean)( mx >= winX + 270.0f && mx <= winX + 470.0f && my >= contentY + 200.0f && my <= contentY + 233.0f ), redTint );
		SCR_UIRowEnd();

	} else if ( g_rpgMenu.tab == 2 ) {
		// ---- RANKS
		SCR_DrawVirtualString( winX + 25.0f, contentY - 2.0f, 4.6f, "^5SERVER RANKS & ELO THRESHOLDS", white );

		static const rpgRankEntry_t defaultRanks[5] = {
			{ 0, "^3Youngling" }, { 1500, "^2Padawan" }, { 2000, "^5Jedi Knight" },
			{ 2500, "^6Jedi Master" }, { 3000, "^1Grand Master" }
		};
		const rpgRankEntry_t *ranks = g_rpgRankCount > 0 ? g_rpgRanks : defaultRanks;
		const int rankCount = g_rpgRankCount > 0 ? g_rpgRankCount : 5;

		const int myElo = g_rpgStats.fr > 0 ? g_rpgStats.fr : ( cg_rpg_fr ? cg_rpg_fr->integer : 1000 );
		int myRank = 0;
		for ( int r = 0; r < rankCount; r++ ) {
			if ( myElo >= ranks[r].threshold ) myRank = r;
		}

		float ry = contentY + 22.0f;
		for ( int r = 0; r < rankCount; r++ ) {
			SCR_UIRowBeginMs( tabAge, r );
			vec4_t rBg = { 1.0f, 1.0f, 1.0f, ( r % 2 == 0 ) ? 0.05f : 0.02f };
			if ( r == myRank ) { rBg[0] = tint[0]; rBg[1] = tint[1]; rBg[2] = tint[2]; rBg[3] = 0.16f; }
			SCR_FillRect( winX + 20.0f, ry, winW - 40.0f, 26.0f, rBg );
			if ( r == myRank ) {
				vec4_t bar = { tint[0], tint[1], tint[2], 0.6f + 0.4f * sinf( (float)now * 0.006f ) };
				SCR_FillRect( winX + 20.0f, ry, 3.0f, 26.0f, bar );
			}
			const char *range = ( r + 1 < rankCount )
				? va( "^2%d - %d Elo", ranks[r].threshold, ranks[r + 1].threshold - 1 )
				: va( "^2%d+ Elo", ranks[r].threshold );
			SCR_DrawVirtualString( winX + 32.0f, ry + 6.0f, 5.2f, ranks[r].title, white );
			SCR_DrawVirtualString( winX + 200.0f, ry + 6.0f, 5.0f, range, white );
			if ( r == myRank ) {
				const char *you = va( "^3YOU  %d Elo", myElo );
				SCR_DrawVirtualString( winX + winW - 32.0f - SCR_GetStringWidth( you, 4.6f ), ry + 6.5f, 4.6f, you, white );
			}
			SCR_UIRowEnd();
			ry += 30.0f;
		}

		const int perLevel = g_rpgXpPerLevel > 0 ? g_rpgXpPerLevel : 1000;
		SCR_DrawVirtualString( winX + 25.0f, ry + 14.0f, 4.4f, va( "^7Level = total XP / ^3%d^7 + 1  (every ^3%d XP^7 is one level)", perLevel, perLevel ), white );

	} else if ( g_rpgMenu.tab == 3 ) {
		// ---- COMMANDS
		SCR_DrawVirtualString( winX + 25.0f, contentY - 2.0f, 4.6f, "^5ALL SERVER COMMANDS (!help / !cmds)", white );

		const char *cmdList[][2] = {
			{ "/login <u> <p>", "Register or log into your account" },
			{ "!details / !myinfo", "View account credentials & display name" },
			{ "!stats / !rank / !ranks", "Stats, rank title & Elo thresholds" },
			{ "!top / !topcredits", "Global top leaderboards" },
			{ "!quests / !achievements", "Daily quests & achievement rewards" },
			{ "!shop / !buy / !sell / !inv", "Economy shop & item inventory" },
			{ "!bounty / !bet / !roll", "Bounties, duel bets & tiered roll gamble" },
			{ "!adventure / !party", "Text adventures & party studio invite" },
			{ "!admin / !freeze / !goto", "High admin controls & player management" }
		};

		float cy = contentY + 20.0f;
		for ( int c = 0; c < 9; c++ ) {
			SCR_UIRowBeginMs( tabAge, c );
			vec4_t zebra = { 1.0f, 1.0f, 1.0f, ( c % 2 == 0 ) ? 0.05f : 0.02f };
			SCR_FillRect( winX + 20.0f, cy, winW - 40.0f, 22.0f, zebra );
			SCR_DrawVirtualString( winX + 30.0f, cy + 6.0f, 4.6f, va( "^3%s", cmdList[c][0] ), white );
			SCR_DrawVirtualString( winX + 230.0f, cy + 6.0f, 4.4f, va( "^7%s", cmdList[c][1] ), white );
			SCR_UIRowEnd();
			cy += 24.0f;
		}
	}

	// back to the window's own fade for the footer
	s_uiAlpha = s_uiBaseAlpha = SCR_UIEase( (float)s_uiAnimMs / 260.0f );
	s_uiDY = s_uiBaseDY = -( 1.0f - s_uiAlpha ) * 16.0f;
	SCR_UIFooter( winX, winY, winW, winH, tint, "^7Press ^1ESC^7 to close the Master Hub" );
	(void)dim;
	SCR_UIAnimEnd( 12 );
}

// Global Party Studio Team Name & Color Selection State
char g_partyTeamName[32] = "Team";
int  g_partyColorIdx = 0; // 0 = Blue, 1 = Red, 2 = Green, 3 = Yellow, 4 = Purple, 5 = Orange, 6 = Black, 7 = White
qboolean s_partyEditingName = qfalse; // true when user is typing in the party name field
int g_adminPlayerScroll = 0;
int g_partyPlayerScroll = 0;
char g_adminCreditsInput[16] = "1000";
char g_adminEloInput[16] = "1500";
char g_adminRankInput[32] = "Jedi Knight";
char g_adminCpInput[64] = "Hello Server!";
qboolean s_adminEditingCredits = qfalse;
qboolean s_adminEditingElo = qfalse;
qboolean s_adminEditingRank = qfalse;
qboolean s_adminEditingCp = qfalse;

int g_partyStudioTab = 0; // 0 = MY PARTY, 1 = ACTIVE PARTIES
int g_pendingJoinReqId = -1;
char g_pendingJoinReqName[64] = "";
int g_pendingInviteLeaderId = -1;
char g_pendingInviteLeaderName[64] = "";
char g_pendingInviteTeamName[64] = "";
int g_clientPartyCount = 0;
clientPartyItem_t g_clientPartyList[32];

/*
==================
SCR_DrawPartyStudioOverlay

Party Management Studio (!party / partymenu)
==================
*/
void SCR_DrawPartyStudioOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgPartyStudio.active ) return;

	float winW = 500.0f;
	float winH = 370.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 240.0f - winH * 0.5f;

	vec4_t bgColor = { 0.03f, 0.07f, 0.14f, 0.94f };
	vec4_t borderColor = { 0.15f, 0.70f, 1.00f, 0.85f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t cyanColor = { 0.20f, 0.85f, 1.00f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_UIGameFrame( winX, winY, winW, winH, bgColor, borderColor, "^5PARTY MANAGEMENT STUDIO", 7.5f, 14.0f );

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	// Tabs Bar: Tab 0: [MY PARTY & INVITES] | Tab 1: [ACTIVE SERVER PARTIES]
	float tabY = winY + 34.0f;
	float tabW = 220.0f;
	float tabH = 20.0f;

	// Tab 0 Button
	qboolean hTab0 = (qboolean)(mx >= winX + 25.0f && mx <= winX + 25.0f + tabW && my >= tabY && my <= tabY + tabH);
	vec4_t t0Bg = { 0.08f, 0.20f, 0.35f, (g_partyStudioTab == 0) ? 0.90f : (hTab0 ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( winX + 25.0f, tabY, tabW, tabH, 3.0f, t0Bg, (g_partyStudioTab == 0) ? borderColor : NULL );
	SCR_DrawVirtualString( winX + 45.0f, tabY + 4.0f, 4.4f, (g_partyStudioTab == 0) ? "^2[ MY PARTY & INVITES ]" : "^7[ MY PARTY & INVITES ]", whiteColor );

	// Tab 1 Button
	qboolean hTab1 = (qboolean)(mx >= winX + 255.0f && mx <= winX + 255.0f + tabW && my >= tabY && my <= tabY + tabH);
	vec4_t t1Bg = { 0.08f, 0.20f, 0.35f, (g_partyStudioTab == 1) ? 0.90f : (hTab1 ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( winX + 255.0f, tabY, tabW, tabH, 3.0f, t1Bg, (g_partyStudioTab == 1) ? borderColor : NULL );
	SCR_DrawVirtualString( winX + 270.0f, tabY + 4.0f, 4.4f, (g_partyStudioTab == 1) ? "^2[ ACTIVE SERVER PARTIES ]" : "^7[ ACTIVE SERVER PARTIES ]", whiteColor );

	SCR_FillRect( winX + 15.0f, winY + 58.0f, winW - 30.0f, 1.0f, borderColor );

	if ( g_partyStudioTab == 0 ) {
		// ==================== TAB 0: MY PARTY & INVITES ====================
		float leftX = winX + 20.0f;
		float leftW = 200.0f;
		float topY = winY + 68.0f;

		SCR_DrawVirtualString( leftX, topY, 4.8f, "^3[YOUR PARTY]", whiteColor );
		SCR_DrawVirtualString( leftX, topY + 18.0f, 4.2f, va("^7Team: ^5%.12s", g_rpgParty.teamName[0] ? g_rpgParty.teamName : "None"), whiteColor );
		SCR_DrawVirtualString( leftX, topY + 32.0f, 4.2f, va("^7Members: ^2%d / %d", g_rpgParty.memberCount, MAX_PARTY_MEMBERS), whiteColor );

		// Party Name Customizer Input Card
		extern qboolean s_partyEditingName;
		qboolean hNameField = (qboolean)(mx >= leftX && mx <= leftX + 180.0f && my >= topY + 48.0f && my <= topY + 66.0f);
		vec4_t nameFieldBg = { 0.01f, 0.03f, 0.07f, 0.80f };
		vec4_t nameFieldBorder = { 0.15f, 0.70f, 1.00f, s_partyEditingName ? 1.00f : (hNameField ? 0.60f : 0.30f) };
		
		SCR_DrawRoundedGlassPanel( leftX, topY + 48.0f, 180.0f, 18.0f, 2.0f, nameFieldBg, nameFieldBorder );
		
		char nameWithCursor[64];
		if ( s_partyEditingName && ((cls.realtime >> 8) & 1) == 0 ) {
			Com_sprintf( nameWithCursor, sizeof(nameWithCursor), "^3%s_", g_partyTeamName );
		} else {
			Com_sprintf( nameWithCursor, sizeof(nameWithCursor), "^3%s", g_partyTeamName );
		}
		SCR_DrawVirtualString( leftX + 6.0f, topY + 53.0f, 4.0f, nameWithCursor, whiteColor );

		// Party Color Picker (8 Shield Colors)
		SCR_DrawVirtualString( leftX, topY + 68.0f, 4.0f, "^7Party Shield Color:", whiteColor );
		vec4_t pColors[8] = {
			{ 0.1f, 0.5f, 1.0f, 1.0f }, // Blue
			{ 1.0f, 0.2f, 0.2f, 1.0f }, // Red
			{ 0.2f, 0.9f, 0.3f, 1.0f }, // Green
			{ 1.0f, 0.9f, 0.2f, 1.0f }, // Yellow
			{ 0.7f, 0.2f, 1.0f, 1.0f }, // Purple
			{ 1.0f, 0.5f, 0.1f, 1.0f }, // Orange
			{ 0.1f, 0.1f, 0.1f, 1.0f }, // Black
			{ 0.9f, 0.9f, 0.9f, 1.0f }  // White
		};

		float colorX = leftX;
		float colorY = topY + 82.0f;
		for ( int c = 0; c < 8; c++ ) {
			float cx = colorX + (c % 4) * 26.0f;
			float cy = colorY + (c / 4) * 22.0f;

			qboolean isSelColor = (qboolean)(g_partyColorIdx == c);
			qboolean hColor = (qboolean)(mx >= cx && mx <= cx + 22.0f && my >= cy && my <= cy + 18.0f);

			SCR_DrawRoundedGlassPanel( cx, cy, 22.0f, 18.0f, 2.0f, pColors[c], isSelColor ? yellowCol : (hColor ? whiteColor : NULL) );
		}

		// Action Buttons
		float btnY = topY + 130.0f;

		// Button 1: Create Party
		qboolean hCreate = (qboolean)(mx >= leftX && mx <= leftX + leftW && my >= btnY && my <= btnY + 26.0f);
		SCR_DrawRoundedGlassPanel( leftX, btnY, leftW, 26.0f, 4.0f, hCreate ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( leftX + 35.0f, btnY + 6.0f, 4.2f, "^2+ CREATE PARTY", whiteColor );

		// Button 2: Leave Party
		btnY += 32.0f;
		qboolean hLeave = (qboolean)(mx >= leftX && mx <= leftX + leftW && my >= btnY && my <= btnY + 26.0f);
		SCR_DrawRoundedGlassPanel( leftX, btnY, leftW, 26.0f, 4.0f, hLeave ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( leftX + 45.0f, btnY + 6.0f, 4.2f, "^1LEAVE PARTY", whiteColor );

		// Button 3: Disband Party
		btnY += 32.0f;
		qboolean hDisband = (qboolean)(mx >= leftX && mx <= leftX + leftW && my >= btnY && my <= btnY + 26.0f);
		SCR_DrawRoundedGlassPanel( leftX, btnY, leftW, 26.0f, 4.0f, hDisband ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( leftX + 40.0f, btnY + 6.0f, 4.2f, "^1DISBAND TEAM", whiteColor );

		// Divider
		SCR_FillRect( winX + 235.0f, winY + 62.0f, 1.0f, winH - 85.0f, borderColor );

		// Right Column: Online Server Players List & One-Click Invite
		float rightX = winX + 245.0f;
		float rightW = winW - 265.0f;

		int csBase = SCR_GetPlayersCSBase();
		int onlineIds[MAX_CLIENTS];
		int onlineTotal = 0;
		for ( int i = 0; i < MAX_CLIENTS; i++ ) {
			if ( i + csBase >= MAX_CONFIGSTRINGS ) break;
			if ( !cl.gameState.stringOffsets[csBase + i] ) continue;
			const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
			if ( !cInfo || !cInfo[0] ) continue;
			char nameBuf[64];
			Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );
			if ( !nameBuf[0] ) continue;
			onlineIds[onlineTotal++] = i;
		}

		int maxVisible = 8;
		int maxScroll = onlineTotal - maxVisible;
		if ( maxScroll < 0 ) maxScroll = 0;
		if ( g_partyPlayerScroll > maxScroll ) g_partyPlayerScroll = maxScroll;
		if ( g_partyPlayerScroll < 0 ) g_partyPlayerScroll = 0;

		char pHeader[64];
		if ( onlineTotal > maxVisible ) {
			Com_sprintf( pHeader, sizeof(pHeader), "^3[ONLINE PLAYERS (%d/%d)] ^5[^7MWHEEL^5]", g_partyPlayerScroll + 1, onlineTotal );
		} else {
			Com_sprintf( pHeader, sizeof(pHeader), "^3[ONLINE PLAYERS (%d)]", onlineTotal );
		}
		SCR_DrawVirtualString( rightX, topY, 4.8f, pHeader, whiteColor );

		float rowY = topY + 20.0f;
		for ( int v = 0; v < maxVisible && (g_partyPlayerScroll + v) < onlineTotal; v++ ) {
			int i = onlineIds[g_partyPlayerScroll + v];
			const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
			char nameBuf[64];
			Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );

			// Draw Player Row
			vec4_t rBg = { 0.05f, 0.15f, 0.25f, (v % 2 == 0) ? 0.35f : 0.15f };
			SCR_FillRect( rightX, rowY, rightW, 22.0f, rBg );

			char displayStr[64];
			Com_sprintf( displayStr, sizeof(displayStr), "^7%s", nameBuf );
			SCR_DrawVirtualString( rightX + 6.0f, rowY + 3.0f, 4.2f, displayStr, whiteColor );

			// [INVITE] Button
			float invBtnX = rightX + rightW - 58.0f;
			float invBtnY = rowY + 2.0f;
			qboolean hInv = (qboolean)(mx >= invBtnX && mx <= invBtnX + 55.0f && my >= invBtnY && my <= invBtnY + 18.0f);

			vec4_t invCol = { 0.00f, 0.60f, 0.20f, hInv ? 0.90f : 0.60f };
			SCR_DrawRoundedGlassPanel( invBtnX, invBtnY, 55.0f, 18.0f, 2.0f, invCol, NULL );
			SCR_DrawVirtualString( invBtnX + 6.0f, invBtnY + 3.0f, 4.0f, "INVITE", whiteColor );

			rowY += 25.0f;
		}

		if ( onlineTotal == 0 ) {
			SCR_DrawVirtualString( rightX + 10.0f, topY + 40.0f, 4.6f, "^7No other players online.", whiteColor );
		}

	} else if ( g_partyStudioTab == 1 ) {
		// ==================== TAB 1: ACTIVE SERVER PARTIES & JOIN REQUESTS ====================
		float contentX = winX + 25.0f;
		float contentW = winW - 50.0f;
		float contentY = winY + 68.0f;

		// 1. Check incoming party invite from another leader
		if ( g_pendingInviteLeaderId >= 0 ) {
			vec4_t invBg = { 0.05f, 0.20f, 0.10f, 0.90f };
			vec4_t invBorder = { 0.20f, 0.90f, 0.40f, 0.95f };
			SCR_DrawRoundedGlassPanel( contentX, contentY, contentW, 26.0f, 4.0f, invBg, invBorder );
			SCR_DrawVirtualString( contentX + 10.0f, contentY + 5.0f, 4.4f, va("^2[PARTY INVITE] ^5%s ^7invited you to join '^3%s^7'!", g_pendingInviteLeaderName[0] ? g_pendingInviteLeaderName : "Leader", g_pendingInviteTeamName[0] ? g_pendingInviteTeamName : "Team"), whiteColor );

			float acBtnX = contentX + contentW - 75.0f;
			float acBtnY = contentY + 3.0f;
			qboolean hAc = (qboolean)(mx >= acBtnX && mx <= acBtnX + 70.0f && my >= acBtnY && my <= acBtnY + 20.0f);
			vec4_t acCol = { 0.00f, 0.65f, 0.25f, hAc ? 0.95f : 0.75f };
			SCR_DrawRoundedGlassPanel( acBtnX, acBtnY, 70.0f, 20.0f, 3.0f, acCol, NULL );
			SCR_DrawVirtualString( acBtnX + 10.0f, acBtnY + 4.0f, 4.2f, "^7[ACCEPT]", whiteColor );

			contentY += 32.0f;
		}

		// 2. Check incoming join request to party leader
		if ( g_pendingJoinReqId >= 0 ) {
			vec4_t reqBg = { 0.20f, 0.12f, 0.02f, 0.85f };
			vec4_t reqBorder = { 1.0f, 0.70f, 0.20f, 0.90f };
			SCR_DrawRoundedGlassPanel( contentX, contentY, contentW, 26.0f, 4.0f, reqBg, reqBorder );
			SCR_DrawVirtualString( contentX + 10.0f, contentY + 5.0f, 4.4f, va("^3[JOIN REQUEST] ^5%s ^7wants to join your party!", g_pendingJoinReqName[0] ? g_pendingJoinReqName : "Player"), whiteColor );

			float acBtnX = contentX + contentW - 75.0f;
			float acBtnY = contentY + 3.0f;
			qboolean hAc = (qboolean)(mx >= acBtnX && mx <= acBtnX + 70.0f && my >= acBtnY && my <= acBtnY + 20.0f);
			vec4_t acCol = { 0.00f, 0.60f, 0.20f, hAc ? 0.95f : 0.75f };
			SCR_DrawRoundedGlassPanel( acBtnX, acBtnY, 70.0f, 20.0f, 3.0f, acCol, NULL );
			SCR_DrawVirtualString( acBtnX + 10.0f, acBtnY + 4.0f, 4.2f, "^2[ACCEPT]", whiteColor );

			contentY += 32.0f;
		}

		SCR_DrawVirtualString( contentX, contentY, 5.0f, va("^3[ACTIVE SERVER TEAMS (%d)]", g_clientPartyCount), cyanColor );
		contentY += 18.0f;

		// List live active parties from server
		vec4_t pColors[8] = {
			{ 0.1f, 0.5f, 1.0f, 1.0f }, // Blue
			{ 1.0f, 0.2f, 0.2f, 1.0f }, // Red
			{ 0.2f, 0.9f, 0.3f, 1.0f }, // Green
			{ 1.0f, 0.9f, 0.2f, 1.0f }, // Yellow
			{ 0.7f, 0.2f, 1.0f, 1.0f }, // Purple
			{ 1.0f, 0.5f, 0.1f, 1.0f }, // Orange
			{ 0.1f, 0.1f, 0.1f, 1.0f }, // Black
			{ 0.9f, 0.9f, 0.9f, 1.0f }  // White
		};

		for ( int p = 0; p < g_clientPartyCount && p < 6; p++ ) {
			clientPartyItem_t *item = &g_clientPartyList[p];

			vec4_t pBg = { 0.05f, 0.15f, 0.25f, (p % 2 == 0) ? 0.35f : 0.15f };
			SCR_FillRect( contentX, contentY, contentW, 26.0f, pBg );

			// Party shield color pill
			int cIdx = (item->colorIdx >= 0 && item->colorIdx < 8) ? item->colorIdx : 0;
			SCR_FillRect( contentX + 6.0f, contentY + 4.0f, 6.0f, 18.0f, pColors[cIdx] );

			SCR_DrawVirtualString( contentX + 18.0f, contentY + 5.0f, 4.4f, va("^5%s ^7(Leader: ^3%s^7) ^2[%d/6]", item->teamName, item->leaderName, item->memberCount), whiteColor );

			// [REQUEST JOIN] Button
			float reqBtnX = contentX + contentW - 115.0f;
			float reqBtnY = contentY + 3.0f;
			qboolean hReq = (qboolean)(mx >= reqBtnX && mx <= reqBtnX + 110.0f && my >= reqBtnY && my <= reqBtnY + 20.0f);
			vec4_t reqCol = { 0.10f, 0.35f, 0.60f, hReq ? 0.95f : 0.70f };
			SCR_DrawRoundedGlassPanel( reqBtnX, reqBtnY, 110.0f, 20.0f, 3.0f, reqCol, borderColor );
			SCR_DrawVirtualString( reqBtnX + 6.0f, reqBtnY + 4.0f, 4.0f, "REQUEST JOIN", whiteColor );

			contentY += 30.0f;
		}

		if ( g_clientPartyCount == 0 ) {
			SCR_DrawVirtualString( contentX + 10.0f, contentY + 20.0f, 4.6f, "^7No active server parties registered yet. Switch to Tab 1 to create one!", whiteColor );
		}
	}

	SCR_DrawVirtualString( winX + (winW - 180.0f) * 0.5f, winY + winH - 15.0f, 4.0f, "^7Press ^1[ESC]^7 to close Party Studio", whiteColor );
}


/*
==================
SCR_DrawAdminOverlay

Admin Control Panel (!admin / !adminmenu)
==================
*/
void SCR_DrawAdminOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgAdmin.active ) return;

	float winW = 630.0f;
	float winH = 430.0f; // Expanded width and height so full names never get cut
	float winX = 320.0f - winW * 0.5f;
	float winY = 240.0f - winH * 0.5f;

	vec4_t bgColor = { 0.12f, 0.03f, 0.05f, 0.95f };
	vec4_t borderColor = { 0.90f, 0.15f, 0.20f, 0.90f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_UIGameFrame( winX, winY, winW, winH, bgColor, borderColor, "^1ADMIN CONTROL PANEL", 7.5f, 16.0f );

	SCR_FillRect( winX + 15.0f, winY + 42.0f, winW - 30.0f, 1.0f, borderColor );

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	// Left Column: Player Selection List (Scrollable for up to 32 players)
	float leftX = winX + 20.0f;
	float leftW = 230.0f;
	float topY = winY + 52.0f;

	int csBase = SCR_GetPlayersCSBase();
	int onlineIds[MAX_CLIENTS];
	int onlineTotal = 0;
	for ( int i = 0; i < MAX_CLIENTS; i++ ) {
		if ( i + csBase >= MAX_CONFIGSTRINGS ) break;
		if ( !cl.gameState.stringOffsets[csBase + i] ) continue;
		const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
		if ( !cInfo || !cInfo[0] ) continue;
		char nameBuf[64];
		Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );
		if ( !nameBuf[0] ) continue;
		onlineIds[onlineTotal++] = i;
	}

	int maxVisible = 10;
	int maxScroll = onlineTotal - maxVisible;
	if ( maxScroll < 0 ) maxScroll = 0;
	if ( g_adminPlayerScroll > maxScroll ) g_adminPlayerScroll = maxScroll;
	if ( g_adminPlayerScroll < 0 ) g_adminPlayerScroll = 0;

	char aHeader[64];
	if ( onlineTotal > maxVisible ) {
		Com_sprintf( aHeader, sizeof(aHeader), "^3TARGET (%d/%d) ^5[^7MWHEEL^5]:", g_adminPlayerScroll + 1, onlineTotal );
	} else {
		Com_sprintf( aHeader, sizeof(aHeader), "^3SELECT TARGET (%d):", onlineTotal );
	}
	SCR_DrawVirtualString( leftX, topY, 4.6f, aHeader, whiteColor );

	float rowY = topY + 20.0f;
	for ( int v = 0; v < maxVisible && (g_adminPlayerScroll + v) < onlineTotal; v++ ) {
		int i = onlineIds[g_adminPlayerScroll + v];
		const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
		char nameBuf[64];
		Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );

		qboolean isSelected = (qboolean)(g_rpgAdmin.selectedClient == i);
		qboolean isHover = (qboolean)(mx >= leftX && mx <= leftX + leftW && my >= rowY && my <= rowY + 22.0f);

		vec4_t rowBg = { 0.40f, 0.05f, 0.10f, isSelected ? 0.85f : (isHover ? 0.50f : 0.20f) };
		SCR_DrawRoundedGlassPanel( leftX, rowY, leftW, 22.0f, 3.0f, rowBg, isSelected ? yellowCol : NULL );

		char displayStr[64];
		Com_sprintf( displayStr, sizeof(displayStr), isSelected ? "^3#%d %s" : "^7#%d %s", i, nameBuf );
		SCR_DrawVirtualString( leftX + 6.0f, rowY + 3.0f, 4.4f, displayStr, whiteColor );

		rowY += 25.0f;
	}

	// Divider
	SCR_FillRect( winX + 258.0f, winY + 48.0f, 1.0f, winH - 75.0f, borderColor );

	// Right Column: Action Buttons
	float rightX = winX + 270.0f;
	float rightW = winW - 285.0f;

	char selName[64] = "No Target";
	int sel = g_rpgAdmin.selectedClient;
	if ( sel >= 0 ) {
		if ( cl.gameState.stringOffsets[csBase + sel] ) {
			const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + sel ];
			if ( cInfo && cInfo[0] ) {
				Q_strncpyz( selName, Info_ValueForKey( cInfo, "n" ), sizeof( selName ) );
				Q_CleanStr( selName );
			}
		}
	}
	SCR_DrawVirtualString( rightX, topY, 4.8f, va("^3EXECUTE ADMIN COMMAND (%s):", selName), whiteColor );

	// Admin Command Action Grid Buttons (12 Commands Grid)
	const char *adminCmds[12][2] = {
		{ "!freeze", "Freeze Target" },
		{ "!unfreeze", "Unfreeze Target" },
		{ "!goto", "Teleport To Target" },
		{ "!bring", "Bring Target Here" },
		{ "!jail", "Jail Target (5m)" },
		{ "!unjail", "Unjail Target" },
		{ "!givecredits", "Give Credits" },
		{ "!setelo", "Set ELO" },
		{ "!setrank", "Set Rank" },
		{ "!cp", "Broadcast Msg" },
		{ "!forcepotato", "Force Hot Potato" },
		{ "!stoppotato", "Stop Hot Potato" }
	};

	float gridY = topY + 20.0f;
	for ( int b = 0; b < 12; b++ ) {
		float bx = rightX + (b % 2) * (140.0f + 10.0f);
		float by = gridY + (b / 2) * 28.0f;

		qboolean bHover = (qboolean)(mx >= bx && mx <= bx + 140.0f && my >= by && my <= by + 22.0f);
		vec4_t btnBg = { 0.35f, 0.08f, 0.12f, bHover ? 0.90f : 0.60f };
		vec4_t btnBorder = { 0.90f, 0.20f, 0.25f, 0.80f };

		SCR_DrawRoundedGlassPanel( bx, by, 140.0f, 22.0f, 4.0f, btnBg, btnBorder );
		SCR_DrawVirtualString( bx + 6.0f, by + 3.0f, 4.2f, va("^1%s", adminCmds[b][0]), whiteColor );
	}

	// Draw Credits Amount Input Box
	float crBoxY = gridY + 175.0f;
	SCR_DrawVirtualString( rightX, crBoxY, 4.4f, "^7Give Credits:", whiteColor );
	qboolean hCr = (qboolean)(mx >= rightX + 100.0f && mx <= rightX + 240.0f && my >= crBoxY - 4.0f && my <= crBoxY + 14.0f);
	vec4_t crBg = { 0.01f, 0.03f, 0.07f, 0.80f };
	vec4_t crBorder = { 0.15f, 0.70f, 1.00f, s_adminEditingCredits ? 1.00f : (hCr ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( rightX + 100.0f, crBoxY - 4.0f, 140.0f, 18.0f, 2.0f, crBg, crBorder );
	char crDisp[32];
	Com_sprintf( crDisp, sizeof(crDisp), s_adminEditingCredits && ((cls.realtime >> 8) & 1) == 0 ? "^3%s_" : "^3%s", g_adminCreditsInput );
	SCR_DrawVirtualString( rightX + 106.0f, crBoxY + 1.0f, 4.4f, crDisp, whiteColor );

	// Draw ELO Input Box
	float eloBoxY = crBoxY + 20.0f;
	SCR_DrawVirtualString( rightX, eloBoxY, 4.4f, "^7Set ELO Val:", whiteColor );
	qboolean hElo = (qboolean)(mx >= rightX + 100.0f && mx <= rightX + 240.0f && my >= eloBoxY - 4.0f && my <= eloBoxY + 14.0f);
	vec4_t eloBg = { 0.01f, 0.03f, 0.07f, 0.80f };
	vec4_t eloBorder = { 0.15f, 0.70f, 1.00f, s_adminEditingElo ? 1.00f : (hElo ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( rightX + 100.0f, eloBoxY - 4.0f, 140.0f, 18.0f, 2.0f, eloBg, eloBorder );
	char eloDisp[32];
	Com_sprintf( eloDisp, sizeof(eloDisp), s_adminEditingElo && ((cls.realtime >> 8) & 1) == 0 ? "^3%s_" : "^3%s", g_adminEloInput );
	SCR_DrawVirtualString( rightX + 106.0f, eloBoxY + 1.0f, 4.4f, eloDisp, whiteColor );

	// Draw Rank Input Box
	float rankBoxY = eloBoxY + 20.0f;
	SCR_DrawVirtualString( rightX, rankBoxY, 4.4f, "^7Set Rank Title:", whiteColor );
	qboolean hRank = (qboolean)(mx >= rightX + 100.0f && mx <= rightX + 240.0f && my >= rankBoxY - 4.0f && my <= rankBoxY + 14.0f);
	vec4_t rankBg = { 0.01f, 0.03f, 0.07f, 0.80f };
	vec4_t rankBorder = { 0.15f, 0.70f, 1.00f, s_adminEditingRank ? 1.00f : (hRank ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( rightX + 100.0f, rankBoxY - 4.0f, 140.0f, 18.0f, 2.0f, rankBg, rankBorder );
	char rankDisp[48];
	Com_sprintf( rankDisp, sizeof(rankDisp), s_adminEditingRank && ((cls.realtime >> 8) & 1) == 0 ? "^3%s_" : "^3%s", g_adminRankInput );
	SCR_DrawVirtualString( rightX + 106.0f, rankBoxY + 1.0f, 4.4f, rankDisp, whiteColor );

	// Draw Broadcast !cp Input Box
	float cpBoxY = rankBoxY + 20.0f;
	SCR_DrawVirtualString( rightX, cpBoxY, 4.4f, "^7Broadcast !cp:", whiteColor );
	qboolean hCp = (qboolean)(mx >= rightX + 100.0f && mx <= rightX + 240.0f && my >= cpBoxY - 4.0f && my <= cpBoxY + 14.0f);
	vec4_t cpBg = { 0.01f, 0.03f, 0.07f, 0.80f };
	vec4_t cpBorder = { 0.15f, 0.70f, 1.00f, s_adminEditingCp ? 1.00f : (hCp ? 0.60f : 0.30f) };
	SCR_DrawRoundedGlassPanel( rightX + 100.0f, cpBoxY - 4.0f, 140.0f, 18.0f, 2.0f, cpBg, cpBorder );
	char cpDisp[64];
	Com_sprintf( cpDisp, sizeof(cpDisp), s_adminEditingCp && ((cls.realtime >> 8) & 1) == 0 ? "^3%s_" : "^3%s", g_adminCpInput );
	SCR_DrawVirtualString( rightX + 106.0f, cpBoxY + 1.0f, 4.4f, cpDisp, whiteColor );

	SCR_DrawVirtualString( winX + (winW - 190.0f) * 0.5f, winY + winH - 15.0f, 4.0f, "^7Press ^1[ESC]^7 to close Admin Panel", whiteColor );
}

/*
==================
SCR_DrawPartyOverlay

WoW-Style RPG Party HUD Overlay (Left Side)
==================
*/
void SCR_DrawPartyOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgParty.active ) return;

	static cvar_t *cg_partyX = NULL;
	static cvar_t *cg_partyY = NULL;
	if ( !cg_partyX ) cg_partyX = Cvar_Get( "cg_partyX", "15", CVAR_ARCHIVE );
	if ( !cg_partyX ) cg_partyX = Cvar_Get( "cg_partyX", "15", CVAR_ARCHIVE );
	if ( !cg_partyY ) cg_partyY = Cvar_Get( "cg_partyY", "140", CVAR_ARCHIVE );

	int members = g_rpgParty.memberCount;
	if ( members <= 0 ) return;
	if ( members > MAX_PARTY_MEMBERS ) members = MAX_PARTY_MEMBERS;
	float cardW = 120.0f;
	float cardH = 16.0f + (float)members * 24.0f + 4.0f;
	float startX = cg_partyX ? cg_partyX->value : 15.0f;
	float startY = cg_partyY ? cg_partyY->value : 140.0f;

	vec4_t bgCol     = { 0.03f, 0.06f, 0.12f, 0.85f };
	vec4_t borderCol = { 0.20f, 0.65f, 1.00f, 0.80f };
	vec4_t whiteCol  = { 1.00f, 1.00f, 1.00f, 1.00f };
	vec4_t hpBgCol   = { 0.10f, 0.10f, 0.10f, 0.90f };
	vec4_t hpFillCol = { 0.15f, 0.85f, 0.25f, 0.95f };
	vec4_t bpBgCol   = { 0.10f, 0.10f, 0.10f, 0.90f };
	vec4_t bpFillCol = { 0.95f, 0.20f, 0.20f, 0.95f };

	SCR_DrawRoundedGlassPanel( startX, startY, cardW, cardH, 4.0f, bgCol, borderCol );

	// Party Name Header + Score
	char headerStr[64];
	Com_sprintf( headerStr, sizeof( headerStr ), "^5%s ^7[^3%dP^7]", g_rpgParty.teamName[0] ? g_rpgParty.teamName : "Party", g_rpgParty.score );
	SCR_DrawVirtualString( startX + 4.0f, startY + 2.0f, 3.8f, headerStr, whiteCol );

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.40f };
	SCR_FillRect( startX + 3.0f, startY + 14.0f, cardW - 6.0f, 1.0f, divColor );

	// Render Party Members
	float rowY = startY + 16.0f;
	for ( int i = 0; i < members; i++ ) {
		rpgPartyMember_t *m = &g_rpgParty.members[i];

		// Real-time local player Health, FP & BP update
		if ( m->clientNum == cl.snap.ps.clientNum ) {
			m->health = cl.snap.ps.stats[STAT_HEALTH];
			m->maxHealth = cl.snap.ps.stats[STAT_MAX_HEALTH] > 0 ? cl.snap.ps.stats[STAT_MAX_HEALTH] : 100;
			m->fp = cl.snap.ps.fd.forcePower;
			m->maxFP = 100;
			m->bp = (cl.snap.ps.jetpackFuel > 0) ? cl.snap.ps.jetpackFuel : (g_liveCombatBP > 0 ? g_liveCombatBP : ((cl.snap.ps.stats[STAT_ARMOR] > 0) ? cl.snap.ps.stats[STAT_ARMOR] : 100));
			m->maxBP = 100;
		}

		char lineStr[64];
		Com_sprintf( lineStr, sizeof( lineStr ), "^3L%d ^7%s", m->level, m->name[0] ? m->name : "Player" );
		SCR_DrawVirtualString( startX + 4.0f, rowY, 3.6f, lineStr, whiteCol );

		// 3 Mini Bars: Green (HP), Blue (FP), Red (BP)
		float barX = startX + 4.0f;
		float barY = rowY + 10.0f;
		float barW = cardW - 8.0f;
		float barH = 2.0f;

		float hpFrac = ( m->maxHealth > 0 ) ? ( (float)m->health / (float)m->maxHealth ) : 1.0f;
		if ( hpFrac < 0.0f ) hpFrac = 0.0f;
		if ( hpFrac > 1.0f ) hpFrac = 1.0f;

		// 1. HP Bar (Green)
		SCR_FillRect( barX, barY, barW, barH, hpBgCol );
		if ( hpFrac > 0.0f ) {
			SCR_FillRect( barX, barY, barW * hpFrac, barH, hpFillCol );
		}

		// 2. FP Bar (Blue) - Use individual member's FP
		int fp = ( m->clientNum == cl.snap.ps.clientNum ) ? cl.snap.ps.fd.forcePower : m->fp;
		int maxFP = ( m->maxFP > 0 ) ? m->maxFP : 100;
		float fpFrac = (float)fp / (float)maxFP;
		if ( fpFrac < 0.0f ) fpFrac = 0.0f;
		if ( fpFrac > 1.0f ) fpFrac = 1.0f;
		vec4_t fpFillCol = { 0.10f, 0.55f, 0.95f, 0.95f };
		float barY_fp = barY + 2.5f;
		SCR_FillRect( barX, barY_fp, barW, barH, bpBgCol );
		if ( fpFrac > 0.0f ) {
			SCR_FillRect( barX, barY_fp, barW * fpFrac, barH, fpFillCol );
		}

		// 3. BP Bar (Red)
		int curBP = m->bp;
		int maxBP = ( m->maxBP > 0 ) ? m->maxBP : 100;
		float bpFrac = (float)curBP / (float)maxBP;
		if ( bpFrac < 0.0f ) bpFrac = 0.0f;
		if ( bpFrac > 1.0f ) bpFrac = 1.0f;

		float barY2 = barY_fp + 2.5f;
		SCR_FillRect( barX, barY2, barW, barH, bpBgCol );
		if ( bpFrac > 0.0f ) {
			SCR_FillRect( barX, barY2, barW * bpFrac, barH, bpFillCol );
		}

		rowY += 24.0f;
	}
}

/*
==================
SCR_DrawLiveBPTracker

Removed per user request (monitoring now done via Party Mini UI).
==================
*/
void SCR_DrawLiveBPTracker( void ) {
	// Disabled per user request
}

static qhandle_t s_hShieldPics[8] = { 0 };


void SCR_DrawPartyOverheadIcons( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cls.cgameStarted ) return;
	if ( !cl.snap.valid ) return;
	if ( !g_rpgParty.active ) return;
	if ( g_rpgParty.memberCount <= 0 ) return;

	static const char *shieldPaths[8] = {
		"gfx/rpg_hud/shield_blue",
		"gfx/rpg_hud/shield_red",
		"gfx/rpg_hud/shield_green",
		"gfx/rpg_hud/shield_yellow",
		"gfx/rpg_hud/shield_purple",
		"gfx/rpg_hud/shield_orange",
		"gfx/rpg_hud/shield_black",
		"gfx/rpg_hud/shield_white"
	};

	int colorIdx = g_rpgParty.teamColorIdx;
	if ( colorIdx < 0 || colorIdx >= 8 ) colorIdx = 0;

	if ( s_hShieldPics[colorIdx] <= 0 && re && re->RegisterShader ) {
		s_hShieldPics[colorIdx] = re->RegisterShader( shieldPaths[colorIdx] );
	}
	qhandle_t hShield = s_hShieldPics[colorIdx];
	if ( hShield <= 0 ) return;

	for ( int i = 0; i < g_rpgParty.memberCount; i++ ) {
		rpgPartyMember_t *m = &g_rpgParty.members[i];
		int targetNum = m->clientNum;
		if ( targetNum < 0 || targetNum >= MAX_CLIENTS ) continue;
		if ( targetNum == cl.snap.ps.clientNum ) continue;

		vec3_t headPos;
		qboolean found = qfalse;

		for ( int e = 0; e < cl.snap.numEntities; e++ ) {
			int parseIdx = ( cl.snap.parseEntitiesNum + e ) & ( MAX_PARSE_ENTITIES - 1 );
			entityState_t *ent = &cl.parseEntities[parseIdx];
			if ( ent->number == targetNum ) {
				VectorCopy( ent->pos.trBase, headPos );
				headPos[2] += 78.0f; // Higher above head
				found = qtrue;
				break;
			}
		}

		if ( found ) {
			float sx = 0.0f, sy = 0.0f;
			if ( FX_WorldToScreen( headPos, &sx, &sy ) ) {
				vec3_t delta;
				VectorSubtract( headPos, cl.snap.ps.origin, delta );
				float dist = VectorLength( delta );
				float scale = 350.0f / ( dist + 200.0f );
				if ( scale > 1.1f ) scale = 1.1f;
				if ( scale < 0.35f ) scale = 0.35f;

				float iconSize = 32.0f * scale;
				SCR_DrawPic( sx - iconSize * 0.5f, sy - iconSize * 0.5f, iconSize, iconSize, hShield );
			}
		}
	}
}

//=======================================================

/*
=============================================================================
CANTO BIGHT BLACKJACK 21 & STAR WARS PAZAAK 20 (!games / !blackjack / !pazaak)
=============================================================================
*/
cantinaGames_t g_cantinaGames = {
	qfalse, 0,
	{}, 0, {}, 0, {}, 0,
	50, qfalse, qfalse, "Place your bet and press DEAL [Space]!", 0,
	qfalse, 50, 0, 0, 0, 0,
	{}, 0, {}, 0,
	{}, {},
	qfalse, qfalse, qtrue, qfalse,
	"Welcome to Star Wars Pazaak! Place your wager and press DEAL MATCH [Space].",
	"Astromech C-7"
};

static void SCR_Blackjack_Shuffle( void ) {
	int idx = 0;
	for ( int s = 0; s < 4; s++ ) {
		for ( int v = 1; v <= 13; v++ ) {
			g_cantinaGames.deck[idx].suit = s;
			g_cantinaGames.deck[idx].val = v;
			idx++;
		}
	}
	for ( int i = 51; i > 0; i-- ) {
		int j = rand() % (i + 1);
		bjCard_t temp = g_cantinaGames.deck[i];
		g_cantinaGames.deck[i] = g_cantinaGames.deck[j];
		g_cantinaGames.deck[j] = temp;
	}
	g_cantinaGames.deckTop = 0;
}

static bjCard_t SCR_Blackjack_DrawCard( void ) {
	if ( g_cantinaGames.deckTop >= 52 ) {
		SCR_Blackjack_Shuffle();
	}
	return g_cantinaGames.deck[g_cantinaGames.deckTop++];
}

static int SCR_Blackjack_Score( bjCard_t *hand, int count, qboolean hideFirst ) {
	int score = 0;
	int aces = 0;
	int start = hideFirst ? 1 : 0;
	for ( int i = start; i < count; i++ ) {
		int v = hand[i].val;
		if ( v >= 10 ) score += 10;
		else if ( v == 1 ) { score += 11; aces++; }
		else score += v;
	}
	while ( score > 21 && aces > 0 ) {
		score -= 10;
		aces--;
	}
	return score;
}

void SCR_Blackjack_Deal( void ) {
	if ( g_cantinaGames.inRound ) return;
	int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;
	if ( g_cantinaGames.currentBet <= 0 ) g_cantinaGames.currentBet = 10;
	if ( g_cantinaGames.currentBet > credits ) {
		Q_strncpyz( g_cantinaGames.statusMsg, "^1Not enough Credits to place bet!", sizeof( g_cantinaGames.statusMsg ) );
		return;
	}

	SCR_Blackjack_Shuffle();
	g_cantinaGames.playerCardCount = 0;
	g_cantinaGames.dealerCardCount = 0;
	g_cantinaGames.inRound = qtrue;
	g_cantinaGames.dealerRevealed = qfalse;
	g_cantinaGames.lastOutcome = 0;

	if ( g_rpgStats.credits >= g_cantinaGames.currentBet ) {
		g_rpgStats.credits -= g_cantinaGames.currentBet;
	}
	CL_AddReliableCommand( va( "rpg_gameresult 1 %d 0", g_cantinaGames.currentBet ), qfalse );

	g_cantinaGames.playerHand[g_cantinaGames.playerCardCount++] = SCR_Blackjack_DrawCard();
	g_cantinaGames.dealerHand[g_cantinaGames.dealerCardCount++] = SCR_Blackjack_DrawCard();
	g_cantinaGames.playerHand[g_cantinaGames.playerCardCount++] = SCR_Blackjack_DrawCard();
	g_cantinaGames.dealerHand[g_cantinaGames.dealerCardCount++] = SCR_Blackjack_DrawCard();

	int pScore = SCR_Blackjack_Score( g_cantinaGames.playerHand, g_cantinaGames.playerCardCount, qfalse );
	if ( pScore == 21 ) {
		g_cantinaGames.dealerRevealed = qtrue;
		g_cantinaGames.inRound = qfalse;
		g_cantinaGames.lastOutcome = 4;
		int winAmt = (int)(g_cantinaGames.currentBet * 2.5f);
		g_rpgStats.credits += winAmt;
		CL_AddReliableCommand( va( "rpg_gameresult 1 0 %d", winAmt ), qfalse );
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^3* NATURAL BLACKJACK! * ^2Won +%d CR!^7", winAmt );
	} else {
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^7Your total is ^2%d^7. [1] Hit or [2] Stand?", pScore );
	}
}

void SCR_Blackjack_Stand( void ) {
	if ( !g_cantinaGames.inRound ) return;
	g_cantinaGames.dealerRevealed = qtrue;
	g_cantinaGames.inRound = qfalse;

	int pScore = SCR_Blackjack_Score( g_cantinaGames.playerHand, g_cantinaGames.playerCardCount, qfalse );
	int dScore = SCR_Blackjack_Score( g_cantinaGames.dealerHand, g_cantinaGames.dealerCardCount, qfalse );

	while ( dScore < 17 && g_cantinaGames.dealerCardCount < 10 ) {
		g_cantinaGames.dealerHand[g_cantinaGames.dealerCardCount++] = SCR_Blackjack_DrawCard();
		dScore = SCR_Blackjack_Score( g_cantinaGames.dealerHand, g_cantinaGames.dealerCardCount, qfalse );
	}

	if ( dScore > 21 ) {
		g_cantinaGames.lastOutcome = 1;
		int winAmt = g_cantinaGames.currentBet * 2;
		g_rpgStats.credits += winAmt;
		CL_AddReliableCommand( va( "rpg_gameresult 1 0 %d", winAmt ), qfalse );
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^2[WIN] DEALER BUST (%d)! ^7Won ^2+%d CR^7!", dScore, winAmt );
	} else if ( pScore > dScore ) {
		g_cantinaGames.lastOutcome = 1;
		int winAmt = g_cantinaGames.currentBet * 2;
		g_rpgStats.credits += winAmt;
		CL_AddReliableCommand( va( "rpg_gameresult 1 0 %d", winAmt ), qfalse );
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^2[WIN] YOU WIN (%d vs %d)! ^7Won ^2+%d CR^7!", pScore, dScore, winAmt );
	} else if ( dScore > pScore ) {
		g_cantinaGames.lastOutcome = 2;
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^1[LOSS] DEALER WINS (%d vs %d). ^7Lost %d CR.", dScore, pScore, g_cantinaGames.currentBet );
	} else {
		g_cantinaGames.lastOutcome = 3;
		g_rpgStats.credits += g_cantinaGames.currentBet;
		CL_AddReliableCommand( va( "rpg_gameresult 1 0 %d", g_cantinaGames.currentBet ), qfalse );
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^3[PUSH] TIE (%d vs %d). ^7Bet returned.", pScore, dScore );
	}
}

void SCR_Blackjack_Hit( void ) {
	if ( !g_cantinaGames.inRound ) return;
	if ( g_cantinaGames.playerCardCount < 10 ) {
		g_cantinaGames.playerHand[g_cantinaGames.playerCardCount++] = SCR_Blackjack_DrawCard();
	}
	int pScore = SCR_Blackjack_Score( g_cantinaGames.playerHand, g_cantinaGames.playerCardCount, qfalse );
	if ( pScore > 21 ) {
		g_cantinaGames.dealerRevealed = qtrue;
		g_cantinaGames.inRound = qfalse;
		g_cantinaGames.lastOutcome = 5;
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^1[BUST] BUST (%d)! ^7Lost %d Credits.", pScore, g_cantinaGames.currentBet );
	} else if ( pScore == 21 ) {
		SCR_Blackjack_Stand();
	} else {
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^7Your total is ^2%d^7. [1] Hit or [2] Stand?", pScore );
	}
}

void SCR_Blackjack_DoubleDown( void ) {
	if ( !g_cantinaGames.inRound ) return;
	int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;
	if ( credits < g_cantinaGames.currentBet ) {
		Q_strncpyz( g_cantinaGames.statusMsg, "^1Not enough Credits to Double Down!", sizeof( g_cantinaGames.statusMsg ) );
		return;
	}
	g_rpgStats.credits -= g_cantinaGames.currentBet;
	CL_AddReliableCommand( va( "rpg_gameresult 1 %d 0", g_cantinaGames.currentBet ), qfalse );
	g_cantinaGames.currentBet *= 2;

	if ( g_cantinaGames.playerCardCount < 10 ) {
		g_cantinaGames.playerHand[g_cantinaGames.playerCardCount++] = SCR_Blackjack_DrawCard();
	}
	int pScore = SCR_Blackjack_Score( g_cantinaGames.playerHand, g_cantinaGames.playerCardCount, qfalse );
	if ( pScore > 21 ) {
		g_cantinaGames.dealerRevealed = qtrue;
		g_cantinaGames.inRound = qfalse;
		g_cantinaGames.lastOutcome = 5;
		Com_sprintf( g_cantinaGames.statusMsg, sizeof( g_cantinaGames.statusMsg ), "^1[BUST] BUST (%d)! ^7Lost %d Credits.", pScore, g_cantinaGames.currentBet );
	} else {
		SCR_Blackjack_Stand();
	}
}

static void SCR_DrawBlackjackCard( float x, float y, int val, int suit, qboolean isHidden ) {
	float w = 38.0f;
	float h = 54.0f;

	static qhandle_t s_hCardDeck[4][14] = {0};
	static qhandle_t s_hCardBack = 0;

	if ( isHidden ) {
		if ( s_hCardBack <= 0 && re && re->RegisterShaderNoMip ) {
			s_hCardBack = re->RegisterShaderNoMip( "gfx/rpg_hud/cards/card_back" );
		}
		if ( s_hCardBack > 0 ) {
			SCR_DrawPic( x, y, w, h, s_hCardBack );
			return;
		}

		vec4_t backBg = { 0.08f, 0.18f, 0.38f, 0.96f };
		vec4_t backBorder = { 0.25f, 0.60f, 0.95f, 1.0f };
		SCR_DrawRoundedGlassPanel( x, y, w, h, 3.0f, backBg, backBorder );
		SCR_DrawVirtualString( x + 12.0f, y + 20.0f, 6.0f, "^5?", NULL );
		return;
	}

	if ( suit >= 0 && suit < 4 && val >= 1 && val <= 13 ) {
		if ( s_hCardDeck[suit][val] <= 0 && re && re->RegisterShaderNoMip ) {
			const char *sName = (suit == 0) ? "spades" : (suit == 1) ? "hearts" : (suit == 2) ? "diamonds" : "clubs";
			const char *vName = (val == 1) ? "A" : (val == 11) ? "J" : (val == 12) ? "Q" : (val == 13) ? "K" : va( "%d", val );
			s_hCardDeck[suit][val] = re->RegisterShaderNoMip( va( "gfx/rpg_hud/cards/card_%s_%s", sName, vName ) );
		}
		if ( s_hCardDeck[suit][val] > 0 ) {
			SCR_DrawPic( x, y, w, h, s_hCardDeck[suit][val] );
			return;
		}
	}

	vec4_t cardBg = { 0.96f, 0.96f, 0.98f, 0.98f };
	vec4_t cardBorder = { 0.70f, 0.75f, 0.85f, 1.0f };
	SCR_DrawRoundedGlassPanel( x, y, w, h, 3.0f, cardBg, cardBorder );

	qboolean isRed = (suit == 1 || suit == 2) ? qtrue : qfalse;
	const char *colorCode = isRed ? "^1" : "^0";

	const char *valStr;
	if ( val == 1 ) valStr = "A";
	else if ( val == 11 ) valStr = "J";
	else if ( val == 12 ) valStr = "Q";
	else if ( val == 13 ) valStr = "K";
	else valStr = va( "%d", val );

	const char *suitStr;
	if ( suit == 0 ) suitStr = "S";
	else if ( suit == 1 ) suitStr = "H";
	else if ( suit == 2 ) suitStr = "D";
	else suitStr = "C";

	SCR_DrawVirtualString( x + 3.0f, y + 2.0f, 3.2f, va( "%s%s", colorCode, valStr ), NULL );
	SCR_DrawVirtualString( x + 3.0f, y + 10.0f, 2.6f, va( "%s%s", colorCode, suitStr ), NULL );
	SCR_DrawVirtualString( x + 13.0f, y + 20.0f, 5.2f, va( "%s%s", colorCode, suitStr ), NULL );
	SCR_DrawVirtualString( x + w - 11.0f, y + h - 12.0f, 3.2f, va( "%s%s", colorCode, valStr ), NULL );
}

// =============================================================================
// STAR WARS PAZAAK 20 GAME ENGINE
// =============================================================================

static void SCR_Pazaak_StartNewSet( void );
static void SCR_Pazaak_CheckMatchOver( void );
static void SCR_Pazaak_ResolveSet( void );
static void SCR_Pazaak_OpponentTurn( void );
static void SCR_Pazaak_DealPlayerTurn( void );

void SCR_Pazaak_InitMatch( void ) {
	int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;
	if ( g_cantinaGames.pzBet <= 0 ) g_cantinaGames.pzBet = 50;
	if ( credits < g_cantinaGames.pzBet ) {
		Q_strncpyz( g_cantinaGames.pzStatusMsg, "^1Not enough Credits to start Pazaak match!", sizeof( g_cantinaGames.pzStatusMsg ) );
		return;
	}

	g_rpgStats.credits -= g_cantinaGames.pzBet;
	CL_AddReliableCommand( va( "rpg_gameresult 2 %d 0", g_cantinaGames.pzBet ), qfalse );

	g_cantinaGames.pzIsMultiplayer = qfalse;
	g_cantinaGames.pzOpponentClientId = -1;
	g_cantinaGames.pzInMatch = qtrue;
	g_cantinaGames.pzPlayerSetsWon = 0;
	g_cantinaGames.pzOppSetsWon = 0;
	g_cantinaGames.pzTab = 0;
	Q_strncpyz( g_cantinaGames.pzOpponentName, "Astromech C-7", sizeof( g_cantinaGames.pzOpponentName ) );

	// Player Hand (4 side cards: combination of +, -, or ± flip cards)
	int sidePool[8] = { 1, 2, 3, 4, 5, 6, -2, -3 };
	for ( int i = 0; i < 4; i++ ) {
		int r = rand() % 8;
		int val = sidePool[r];
		if ( i == 3 && (rand() % 2 == 0) ) {
			g_cantinaGames.pzPlayerHand[i].val = (rand() % 3) + 1;
			g_cantinaGames.pzPlayerHand[i].isFlip = qtrue;
		} else {
			g_cantinaGames.pzPlayerHand[i].val = val;
			g_cantinaGames.pzPlayerHand[i].isFlip = qfalse;
		}
		g_cantinaGames.pzPlayerHand[i].used = qfalse;
	}

	// Opponent Hand
	for ( int i = 0; i < 4; i++ ) {
		int r = rand() % 8;
		g_cantinaGames.pzOppHand[i].val = sidePool[r];
		g_cantinaGames.pzOppHand[i].isFlip = (rand() % 4 == 0) ? qtrue : qfalse;
		g_cantinaGames.pzOppHand[i].used = qfalse;
	}

	SCR_Pazaak_StartNewSet();
}

void SCR_Pazaak_InitMultiplayerMatch( int oppId, const char *oppName, int bet, qboolean myTurnFirst ) {
	g_cantinaGames.active = qtrue;
	g_cantinaGames.activeGame = 2;
	g_cantinaGames.pzTab = 0;
	g_cantinaGames.pzIsMultiplayer = qtrue;
	g_cantinaGames.pzOpponentClientId = oppId;
	g_cantinaGames.pzInMatch = qtrue;
	g_cantinaGames.pzBet = bet;
	g_cantinaGames.pzPlayerSetsWon = 0;
	g_cantinaGames.pzOppSetsWon = 0;
	g_cantinaGames.pzPendingChallengerId = -1;
	Q_strncpyz( g_cantinaGames.pzOpponentName, oppName ? oppName : "Opponent", sizeof( g_cantinaGames.pzOpponentName ) );

	int sidePool[8] = { 1, 2, 3, 4, 5, 6, -2, -3 };
	for ( int i = 0; i < 4; i++ ) {
		int r = rand() % 8;
		int val = sidePool[r];
		if ( i == 3 && (rand() % 2 == 0) ) {
			g_cantinaGames.pzPlayerHand[i].val = (rand() % 3) + 1;
			g_cantinaGames.pzPlayerHand[i].isFlip = qtrue;
		} else {
			g_cantinaGames.pzPlayerHand[i].val = val;
			g_cantinaGames.pzPlayerHand[i].isFlip = qfalse;
		}
		g_cantinaGames.pzPlayerHand[i].used = qfalse;
		g_cantinaGames.pzOppHand[i].used = qfalse;
	}

	g_cantinaGames.pzPlayerBoardCount = 0;
	g_cantinaGames.pzOppBoardCount = 0;
	g_cantinaGames.pzPlayerScore = 0;
	g_cantinaGames.pzOppScore = 0;
	g_cantinaGames.pzPlayerStood = qfalse;
	g_cantinaGames.pzOppStood = qfalse;
	g_cantinaGames.pzCardPlayedThisTurn = qfalse;

	if ( myTurnFirst ) {
		g_cantinaGames.pzIsPlayerTurn = qtrue;
		int c = (rand() % 10) + 1;
		g_cantinaGames.pzPlayerBoard[g_cantinaGames.pzPlayerBoardCount++] = c;
		g_cantinaGames.pzPlayerScore = c;
		CL_AddReliableCommand( va( "rpg_pazaak_sync deal %d", c ), qfalse );
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^7Your Turn! Drew ^2%d^7 (Score: ^2%d^7). Play card, [Space] End Turn, [S] Stand.", c, c );
	} else {
		g_cantinaGames.pzIsPlayerTurn = qfalse;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^5%s^7's turn. Waiting for opponent move...", g_cantinaGames.pzOpponentName );
	}
}

void SCR_Pazaak_HandleOpponentAction( const char *actionStr ) {
	if ( !actionStr || !g_cantinaGames.pzInMatch ) return;

	char action[32] = "";
	int p1 = 0, p2 = 0;
	sscanf( actionStr, "%31s %d %d", action, &p1, &p2 );

	if ( !Q_stricmp( action, "deal" ) ) {
		if ( g_cantinaGames.pzOppBoardCount < 9 ) {
			g_cantinaGames.pzOppBoard[g_cantinaGames.pzOppBoardCount++] = p1;
			g_cantinaGames.pzOppScore += p1;
		}
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^5%s ^7drew ^3%d ^7(Score: ^3%d^7).", g_cantinaGames.pzOpponentName, p1, g_cantinaGames.pzOppScore );
	} else if ( !Q_stricmp( action, "playcard" ) ) {
		int cardIdx = p1;
		int val = p2;
		if ( cardIdx >= 0 && cardIdx < 4 ) {
			g_cantinaGames.pzOppHand[cardIdx].used = qtrue;
		}
		g_cantinaGames.pzOppScore += val;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^5%s ^7played %s%d (Score: ^3%d^7).", g_cantinaGames.pzOpponentName, val >= 0 ? "+" : "", val, g_cantinaGames.pzOppScore );
	} else if ( !Q_stricmp( action, "stand" ) ) {
		g_cantinaGames.pzOppStood = qtrue;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^5%s ^1STOOD ^7with score ^3%d^7!", g_cantinaGames.pzOpponentName, g_cantinaGames.pzOppScore );
		if ( g_cantinaGames.pzPlayerStood ) {
			SCR_Pazaak_ResolveSet();
		} else {
			SCR_Pazaak_DealPlayerTurn();
		}
	} else if ( !Q_stricmp( action, "endturn" ) ) {
		if ( g_cantinaGames.pzOppScore > 20 ) {
			g_cantinaGames.pzPlayerSetsWon++;
			Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[WIN] %s BUSTED (%d)! ^7You won the set!", g_cantinaGames.pzOpponentName, g_cantinaGames.pzOppScore );
			SCR_Pazaak_CheckMatchOver();
			return;
		}
		if ( !g_cantinaGames.pzPlayerStood ) {
			SCR_Pazaak_DealPlayerTurn();
		}
	} else if ( !Q_stricmp( action, "forfeit" ) ) {
		g_cantinaGames.pzInMatch = qfalse;
		int pot = g_cantinaGames.pzBet * 2;
		g_rpgStats.credits += pot;
		CL_AddReliableCommand( va( "rpg_pazaak_endmatch %d %d %d", cl.snap.ps.clientNum, g_cantinaGames.pzOpponentClientId, pot ), qfalse );
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[VICTORY] %s FORFEITED! ^7You won the ^3%d CR ^7pot!", g_cantinaGames.pzOpponentName, pot );
	}
}

static void SCR_Pazaak_StartNewSet( void ) {
	g_cantinaGames.pzPlayerBoardCount = 0;
	g_cantinaGames.pzOppBoardCount = 0;
	g_cantinaGames.pzPlayerScore = 0;
	g_cantinaGames.pzOppScore = 0;
	g_cantinaGames.pzPlayerStood = qfalse;
	g_cantinaGames.pzOppStood = qfalse;
	g_cantinaGames.pzCardPlayedThisTurn = qfalse;
	g_cantinaGames.pzIsPlayerTurn = qtrue;

	int c = (rand() % 10) + 1;
	g_cantinaGames.pzPlayerBoard[g_cantinaGames.pzPlayerBoardCount++] = c;
	g_cantinaGames.pzPlayerScore = c;
	if ( g_cantinaGames.pzIsMultiplayer ) {
		CL_AddReliableCommand( va( "rpg_pazaak_sync deal %d", c ), qfalse );
	}
	Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^7Drew ^2%d^7 (Score: ^2%d^7). Play card [1-4], [Space] End Turn, [S] Stand.", c, c );
}

void SCR_Pazaak_PlayCard( int cardIdx ) {
	if ( !g_cantinaGames.pzInMatch || !g_cantinaGames.pzIsPlayerTurn || g_cantinaGames.pzPlayerStood || g_cantinaGames.pzCardPlayedThisTurn ) return;
	if ( cardIdx < 0 || cardIdx >= 4 || g_cantinaGames.pzPlayerHand[cardIdx].used ) return;

	pzHandCard_t *c = &g_cantinaGames.pzPlayerHand[cardIdx];
	g_cantinaGames.pzPlayerScore += c->val;
	c->used = qtrue;
	g_cantinaGames.pzCardPlayedThisTurn = qtrue;

	if ( g_cantinaGames.pzIsMultiplayer ) {
		CL_AddReliableCommand( va( "rpg_pazaak_sync playcard %d %d", cardIdx, c->val ), qfalse );
	}

	if ( g_cantinaGames.pzPlayerScore > 20 ) {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1Score is %d (BUST)! ^7End turn or play another card.", g_cantinaGames.pzPlayerScore );
	} else if ( g_cantinaGames.pzPlayerScore == 20 ) {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[20 POINTS!] ^7Press [S] or [1] to Stand!" );
	} else {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^7Played %s%d (Score: ^2%d^7). Press [Space] End Turn or [S] Stand.", c->val >= 0 ? "+" : "", c->val, g_cantinaGames.pzPlayerScore );
	}
}

void SCR_Pazaak_FlipHandCard( int cardIdx ) {
	if ( cardIdx < 0 || cardIdx >= 4 ) return;
	pzHandCard_t *c = &g_cantinaGames.pzPlayerHand[cardIdx];
	if ( !c->used && c->isFlip ) {
		c->val = -c->val;
	}
}

void SCR_Pazaak_Stand( void ) {
	if ( !g_cantinaGames.pzInMatch || !g_cantinaGames.pzIsPlayerTurn || g_cantinaGames.pzPlayerStood ) return;
	g_cantinaGames.pzPlayerStood = qtrue;
	g_cantinaGames.pzIsPlayerTurn = qfalse;

	if ( g_cantinaGames.pzIsMultiplayer ) {
		CL_AddReliableCommand( "rpg_pazaak_sync stand", qfalse );
	}

	if ( g_cantinaGames.pzOppStood ) {
		SCR_Pazaak_ResolveSet();
	} else {
		if ( !g_cantinaGames.pzIsMultiplayer ) {
			SCR_Pazaak_OpponentTurn();
		}
	}
}

void SCR_Pazaak_EndTurn( void ) {
	if ( !g_cantinaGames.pzInMatch || !g_cantinaGames.pzIsPlayerTurn || g_cantinaGames.pzPlayerStood ) return;

	if ( g_cantinaGames.pzIsMultiplayer ) {
		CL_AddReliableCommand( "rpg_pazaak_sync endturn", qfalse );
	}

	if ( g_cantinaGames.pzPlayerScore > 20 ) {
		g_cantinaGames.pzOppSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1[BUST] BUST (%d)! ^7%s takes the set!", g_cantinaGames.pzPlayerScore, g_cantinaGames.pzOpponentName );
		SCR_Pazaak_CheckMatchOver();
		return;
	}

	g_cantinaGames.pzIsPlayerTurn = qfalse;
	if ( !g_cantinaGames.pzIsMultiplayer ) {
		if ( !g_cantinaGames.pzOppStood ) {
			SCR_Pazaak_OpponentTurn();
		} else {
			SCR_Pazaak_DealPlayerTurn();
		}
	}
}

static void SCR_Pazaak_DealPlayerTurn( void ) {
	if ( g_cantinaGames.pzPlayerStood ) {
		if ( g_cantinaGames.pzOppStood ) SCR_Pazaak_ResolveSet();
		else if ( !g_cantinaGames.pzIsMultiplayer ) SCR_Pazaak_OpponentTurn();
		return;
	}

	g_cantinaGames.pzIsPlayerTurn = qtrue;
	g_cantinaGames.pzCardPlayedThisTurn = qfalse;

	if ( g_cantinaGames.pzPlayerBoardCount >= 9 ) {
		SCR_Pazaak_ResolveSet();
		return;
	}

	int c = (rand() % 10) + 1;
	g_cantinaGames.pzPlayerBoard[g_cantinaGames.pzPlayerBoardCount++] = c;
	g_cantinaGames.pzPlayerScore += c;

	if ( g_cantinaGames.pzIsMultiplayer ) {
		CL_AddReliableCommand( va( "rpg_pazaak_sync deal %d", c ), qfalse );
	}

	if ( g_cantinaGames.pzPlayerBoardCount == 9 && g_cantinaGames.pzPlayerScore <= 20 ) {
		g_cantinaGames.pzPlayerSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^3[AUTO-WIN] 9-CARD FULL GRID! ^2Won the set!^7" );
		SCR_Pazaak_CheckMatchOver();
		return;
	}

	if ( g_cantinaGames.pzPlayerScore > 20 ) {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1Total is %d (BUST)! ^7Play a minus side card [1-4] or forfeit set.", g_cantinaGames.pzPlayerScore );
	} else if ( g_cantinaGames.pzPlayerScore == 20 ) {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[20 POINTS!] ^7Press [S] or [1] to Stand!" );
	} else {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^7Drew ^2%d ^7(Score: ^2%d^7). Play card [1-4], [Space] End Turn, [S] Stand.", c, g_cantinaGames.pzPlayerScore );
	}
}

static void SCR_Pazaak_OpponentTurn( void ) {
	if ( g_cantinaGames.pzOppStood ) {
		if ( !g_cantinaGames.pzPlayerStood ) SCR_Pazaak_DealPlayerTurn();
		else SCR_Pazaak_ResolveSet();
		return;
	}

	if ( g_cantinaGames.pzOppBoardCount >= 9 ) {
		SCR_Pazaak_ResolveSet();
		return;
	}

	int c = (rand() % 10) + 1;
	g_cantinaGames.pzOppBoard[g_cantinaGames.pzOppBoardCount++] = c;
	g_cantinaGames.pzOppScore += c;

	// AI Hand Decision
	if ( g_cantinaGames.pzOppScore > 20 ) {
		for ( int i = 0; i < 4; i++ ) {
			if ( !g_cantinaGames.pzOppHand[i].used && g_cantinaGames.pzOppHand[i].val < 0 && g_cantinaGames.pzOppScore + g_cantinaGames.pzOppHand[i].val <= 20 ) {
				g_cantinaGames.pzOppScore += g_cantinaGames.pzOppHand[i].val;
				g_cantinaGames.pzOppHand[i].used = qtrue;
				break;
			}
		}
	}

	if ( g_cantinaGames.pzOppScore > 20 ) {
		g_cantinaGames.pzPlayerSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[WIN] %s BUSTED (%d)! ^7You won the set!", g_cantinaGames.pzOpponentName, g_cantinaGames.pzOppScore );
		SCR_Pazaak_CheckMatchOver();
		return;
	}

	if ( g_cantinaGames.pzOppScore >= 18 || (g_cantinaGames.pzPlayerStood && g_cantinaGames.pzOppScore > g_cantinaGames.pzPlayerScore) ) {
		g_cantinaGames.pzOppStood = qtrue;
	}

	if ( g_cantinaGames.pzPlayerStood && g_cantinaGames.pzOppStood ) {
		SCR_Pazaak_ResolveSet();
	} else if ( !g_cantinaGames.pzPlayerStood ) {
		SCR_Pazaak_DealPlayerTurn();
	} else {
		SCR_Pazaak_OpponentTurn();
	}
}

static void SCR_Pazaak_ResolveSet( void ) {
	int p = g_cantinaGames.pzPlayerScore;
	int o = g_cantinaGames.pzOppScore;

	if ( p > 20 && o > 20 ) {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^3[TIE] TIE SET (%d vs %d). Both busted.", p, o );
	} else if ( p > 20 ) {
		g_cantinaGames.pzOppSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1[BUST] BUST (%d vs %d). ^7%s takes the set.", p, o, g_cantinaGames.pzOpponentName );
	} else if ( o > 20 ) {
		g_cantinaGames.pzPlayerSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[WIN] OPPONENT BUST (%d vs %d)! ^7You take the set!", p, o );
	} else if ( p > o ) {
		g_cantinaGames.pzPlayerSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^2[WIN] SET WON (%d vs %d)! ^7You take the set!", p, o );
	} else if ( o > p ) {
		g_cantinaGames.pzOppSetsWon++;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1[LOSS] SET LOST (%d vs %d). ^7%s takes the set.", p, o, g_cantinaGames.pzOpponentName );
	} else {
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^3[TIE] TIE SET (%d vs %d). Score equal.", p, o );
	}

	SCR_Pazaak_CheckMatchOver();
}

static void SCR_Pazaak_CheckMatchOver( void ) {
	if ( g_cantinaGames.pzPlayerSetsWon >= 3 ) {
		g_cantinaGames.pzInMatch = qfalse;
		int winAmt = g_cantinaGames.pzBet * 2;
		g_rpgStats.credits += winAmt;
		if ( g_cantinaGames.pzIsMultiplayer ) {
			CL_AddReliableCommand( va( "rpg_pazaak_endmatch %d %d %d", cl.snap.ps.clientNum, g_cantinaGames.pzOpponentClientId, winAmt ), qfalse );
		} else {
			CL_AddReliableCommand( va( "rpg_gameresult 2 0 %d", winAmt ), qfalse );
		}
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^3[VICTORY] MATCH VICTORY (3-%d)! ^2Won +%d Credits! ^7Press [Space] for New Match.", g_cantinaGames.pzOppSetsWon, winAmt );
	} else if ( g_cantinaGames.pzOppSetsWon >= 3 ) {
		g_cantinaGames.pzInMatch = qfalse;
		Com_sprintf( g_cantinaGames.pzStatusMsg, sizeof( g_cantinaGames.pzStatusMsg ), "^1[DEFEAT] MATCH DEFEAT (%d-3). ^7Lost %d Credits. Press [Space] for New Match.", g_cantinaGames.pzPlayerSetsWon, g_cantinaGames.pzBet );
	} else {
		SCR_Pazaak_StartNewSet();
	}
}

static void SCR_DrawPazaakMiniCard( float x, float y, float w, float h, int val, qboolean isSideCard, qboolean isUsed, qboolean isFlip ) {
	if ( isUsed ) {
		vec4_t usedBg = { 0.05f, 0.08f, 0.12f, 0.40f };
		vec4_t usedBorder = { 0.20f, 0.25f, 0.35f, 0.40f };
		qboolean isNeg = (val < 0) ? qtrue : qfalse;
		vec4_t pillBg = { isNeg ? 0.65f : 0.10f, isNeg ? 0.12f : 0.45f, isNeg ? 0.18f : 0.85f, 0.95f };
		vec4_t pillBorder = { isFlip ? 1.0f : 0.8f, isFlip ? 0.85f : 0.9f, isFlip ? 0.2f : 1.0f, 1.0f };
		SCR_DrawRoundedGlassPanel( x, y, w, h, 4.0f, pillBg, pillBorder );

		const char *signStr = (val > 0) ? "+" : "";
		char cardStr[16];
		Com_sprintf( cardStr, sizeof(cardStr), "%s%d", signStr, val );
		float strW = SCR_GetStringWidth( cardStr, 4.2f );
		SCR_DrawVirtualString( x + (w - strW) * 0.5f, y + h * 0.5f - 5.0f, 4.2f, cardStr, NULL );
		if ( isFlip ) {
			SCR_DrawVirtualString( x + w - 8.0f, y + 2.0f, 2.4f, "^3±", NULL );
		}
	} else {
		// Green Main Board Card (1-10)
		vec4_t greenBg = { 0.08f, 0.45f, 0.22f, 0.95f };
		vec4_t greenBorder = { 0.35f, 0.95f, 0.55f, 1.0f };
		SCR_DrawRoundedGlassPanel( x, y, w, h, 4.0f, greenBg, greenBorder );

		char cardStr[16];
		Com_sprintf( cardStr, sizeof(cardStr), "%d", val );
		float strW = SCR_GetStringWidth( cardStr, 4.8f );
		SCR_DrawVirtualString( x + (w - strW) * 0.5f, y + h * 0.5f - 5.0f, 4.8f, cardStr, NULL );
	}
}

/*
==================
SCR_DrawCantinaGamesOverlay

Main Cantina Games Hub, Canto Bight Blackjack 21, and Star Wars Pazaak 20 overlay
==================
*/
void SCR_DrawCantinaGamesOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_cantinaGames.active ) return;

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t cyanColor = { 0.10f, 0.80f, 1.00f, 1.0f };
	int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;

	// ========================================================
	// VIEW 0: CANTINA GAMES HUB SELECTOR
	// ========================================================
	if ( g_cantinaGames.activeGame == 0 ) {
		float winW = 500.0f;
		float winH = 340.0f;
		float winX = 320.0f - winW * 0.5f;
		float winY = 240.0f - winH * 0.5f;

		vec4_t hubBg = { 0.04f, 0.08f, 0.16f, 0.95f };
		vec4_t hubBorder = { 0.10f, 0.75f, 0.95f, 0.90f };
		SCR_UIGameFrame( winX, winY, winW, winH, hubBg, hubBorder, "^3GALACTIC CANTINA GAMES HUB", 7.5f, 16.0f );
		SCR_DrawVirtualString( winX + 20.0f, winY + 38.0f, 4.4f, va( "^7Your Bankroll: ^5%d Credits", credits ), cyanColor );

		// Card 1: Canto Bight Blackjack 21
		float c1X = winX + 25.0f;
		float c1Y = winY + 65.0f;
		float c1W = winW - 50.0f;
		float c1H = 110.0f;

		qboolean c1Hover = (mx >= c1X && mx <= c1X + c1W && my >= c1Y && my <= c1Y + c1H) ? qtrue : qfalse;
		vec4_t c1Bg = { 0.03f, 0.18f, 0.12f, c1Hover ? 0.95f : 0.75f };
		vec4_t c1Border = { 0.95f, 0.75f, 0.10f, c1Hover ? 1.0f : 0.60f };
		SCR_DrawRoundedGlassPanel( c1X, c1Y, c1W, c1H, 6.0f, c1Bg, c1Border );

		SCR_DrawVirtualString( c1X + 16.0f, c1Y + 14.0f, 6.0f, "^31. CANTO BIGHT BLACKJACK 21", yellowCol );
		SCR_DrawVirtualString( c1X + 16.0f, c1Y + 34.0f, 3.8f, "^7Classic casino table vs Croupier Droid. Natural Blackjack pays 3:2!", whiteColor );
		SCR_DrawVirtualString( c1X + 16.0f, c1Y + 48.0f, 3.8f, "^7Wager from ^310 ^7to ^3500 Credits^7 per hand. Double Down on strong totals!", whiteColor );
		
		static qhandle_t s_hBtnPlayTable = 0, s_hBtnPlayTableHov = 0;
		if ( s_hBtnPlayTable <= 0 && re && re->RegisterShaderNoMip ) {
			s_hBtnPlayTable = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_play_table" );
			s_hBtnPlayTableHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_play_table_hover" );
		}
		qhandle_t hPlay1 = c1Hover ? s_hBtnPlayTableHov : s_hBtnPlayTable;
		{
			vec4_t tint = { 0.95f, 0.75f, 0.10f, 1.0f };
			SCR_UIButton( c1X + c1W - 130.0f, c1Y + 68.0f, 115.0f, 30.0f, "PLAY TABLE", 4.6f, c1Hover, tint );
		}

		// Card 2: Star Wars Pazaak 20
		float c2X = winX + 25.0f;
		float c2Y = winY + 190.0f;
		float c2W = winW - 50.0f;
		float c2H = 110.0f;

		qboolean c2Hover = (mx >= c2X && mx <= c2X + c2W && my >= c2Y && my <= c2Y + c2H) ? qtrue : qfalse;
		vec4_t c2Bg = { 0.05f, 0.12f, 0.22f, c2Hover ? 0.95f : 0.75f };
		vec4_t c2Border = { 0.20f, 0.70f, 0.95f, c2Hover ? 1.0f : 0.60f };
		SCR_DrawRoundedGlassPanel( c2X, c2Y, c2W, c2H, 6.0f, c2Bg, c2Border );

		SCR_DrawVirtualString( c2X + 16.0f, c2Y + 14.0f, 6.0f, "^52. STAR WARS PAZAAK 20", cyanColor );
		SCR_DrawVirtualString( c2X + 16.0f, c2Y + 34.0f, 3.8f, "^7Iconic Star Wars cantina duel. 3x3 main deck grid & 4-card modifier hand.", whiteColor );
		SCR_DrawVirtualString( c2X + 16.0f, c2Y + 48.0f, 3.8f, "^7Best-of-3 set matches against Astromech C-7 or Online players.", whiteColor );

		static qhandle_t s_hBtnPlayPazaak = 0, s_hBtnPlayPazaakHov = 0;
		if ( s_hBtnPlayPazaak <= 0 && re && re->RegisterShaderNoMip ) {
			s_hBtnPlayPazaak = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_play_pazaak" );
			s_hBtnPlayPazaakHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_play_pazaak_hover" );
		}
		qhandle_t hPlay2 = c2Hover ? s_hBtnPlayPazaakHov : s_hBtnPlayPazaak;
		{
			vec4_t tint = { 0.20f, 0.70f, 0.95f, 1.0f };
			SCR_UIButton( c2X + c2W - 130.0f, c2Y + 68.0f, 115.0f, 30.0f, "PLAY PAZAAK", 4.6f, c2Hover, tint );
		}
		return;
	}

	// ========================================================
	// VIEW 1: CANTO BIGHT BLACKJACK 21 TABLE
	// ========================================================
	if ( g_cantinaGames.activeGame == 1 ) {
		float winW = 560.0f;
		float winH = 400.0f;
		float winX = 320.0f - winW * 0.5f;
		float winY = 240.0f - winH * 0.5f;

		vec4_t feltBg = { 0.02f, 0.16f, 0.10f, 0.96f };
		vec4_t feltBorder = { 0.95f, 0.75f, 0.15f, 0.95f };
		SCR_UIGameFrame( winX, winY, winW, winH, feltBg, feltBorder, "^3CANTO BIGHT BLACKJACK 21", 7.0f, 12.0f );
		SCR_DrawVirtualString( winX + 16.0f, winY + 12.0f, 4.2f, "< ^5[BACK]", cyanColor );

		SCR_DrawVirtualString( winX + 20.0f, winY + 30.0f, 3.8f, "^7DEALER STANDS ON 17  -  BLACKJACK PAYS 3:2", yellowCol );
		SCR_DrawVirtualString( winX + winW - 150.0f, winY + 30.0f, 4.4f, va( "^7Bankroll: ^5%d CR", credits ), cyanColor );

		// Dealer Area
		qboolean hideDealerHole = (!g_cantinaGames.dealerRevealed && g_cantinaGames.inRound) ? qtrue : qfalse;
		int dScore = SCR_Blackjack_Score( g_cantinaGames.dealerHand, g_cantinaGames.dealerCardCount, hideDealerHole );
		SCR_DrawVirtualString( winX + 30.0f, winY + 52.0f, 4.4f, va( "^7Croupier Droid Score: ^3%d", dScore ), whiteColor );

		float dCardX = winX + 30.0f;
		float dCardY = winY + 68.0f;
		for ( int i = 0; i < g_cantinaGames.dealerCardCount; i++ ) {
			qboolean hideHole = (i == 0 && !g_cantinaGames.dealerRevealed && g_cantinaGames.inRound) ? qtrue : qfalse;
			SCR_DrawBlackjackCard( dCardX + i * 44.0f, dCardY, g_cantinaGames.dealerHand[i].val, g_cantinaGames.dealerHand[i].suit, hideHole );
		}

		// Table Middle: Status Banner
		float midY = winY + 138.0f;
		vec4_t bannerBg = { 0.01f, 0.05f, 0.08f, 0.70f };
		SCR_DrawRoundedGlassPanel( winX + 20.0f, midY, winW - 40.0f, 34.0f, 6.0f, bannerBg, feltBorder );

		float msgW = SCR_GetStringWidth( g_cantinaGames.statusMsg, 4.4f );
		SCR_DrawVirtualString( winX + (winW - msgW) * 0.5f, midY + 10.0f, 4.4f, g_cantinaGames.statusMsg, whiteColor );

		// Player Area
		int pScore = SCR_Blackjack_Score( g_cantinaGames.playerHand, g_cantinaGames.playerCardCount, qfalse );
		SCR_DrawVirtualString( winX + 30.0f, winY + 184.0f, 4.4f, va( "^7Your Hand Score: ^2%d", pScore ), whiteColor );

		float pCardX = winX + 30.0f;
		float pCardY = winY + 200.0f;
		for ( int i = 0; i < g_cantinaGames.playerCardCount; i++ ) {
			SCR_DrawBlackjackCard( pCardX + i * 44.0f, pCardY, g_cantinaGames.playerHand[i].val, g_cantinaGames.playerHand[i].suit, qfalse );
		}

		// Chip Shaders & Buttons Shaders Registration
		static qhandle_t s_hChips[6] = {0}; // 10, 25, 50, 100, 500, clear
		static qhandle_t s_hBtnDeal = 0, s_hBtnDealHov = 0;
		static qhandle_t s_hBtnHit = 0, s_hBtnHitHov = 0;
		static qhandle_t s_hBtnStand = 0, s_hBtnStandHov = 0;
		static qhandle_t s_hBtnDouble = 0, s_hBtnDoubleHov = 0;
		static qhandle_t s_hBtnClear = 0, s_hBtnClearHov = 0;

		if ( s_hChips[0] <= 0 && re && re->RegisterShaderNoMip ) {
			s_hChips[0] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_10" );
			s_hChips[1] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_25" );
			s_hChips[2] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_50" );
			s_hChips[3] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_100" );
			s_hChips[4] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_500" );
			s_hChips[5] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_clear" );

			s_hBtnDeal = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_deal" );
			s_hBtnDealHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_deal_hover" );
			s_hBtnHit = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_hit" );
			s_hBtnHitHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_hit_hover" );
			s_hBtnStand = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_stand" );
			s_hBtnStandHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_stand_hover" );
			s_hBtnDouble = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_double" );
			s_hBtnDoubleHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_double_hover" );
			s_hBtnClear = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_clear" );
			s_hBtnClearHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_clear_hover" );
		}

		// Chip Selector (3D Circular Chips)
		float chipY = winY + 268.0f;
		float chipSize = 34.0f;
		SCR_DrawVirtualString( winX + 25.0f, chipY + 10.0f, 4.4f, va( "^7Bet: ^3%d CR", g_cantinaGames.currentBet ), yellowCol );

		float chipStartX = winX + 130.0f;
		for ( int c = 0; c < 5; c++ ) {
			float cx = chipStartX + c * 48.0f;
			qboolean chipHover = (!g_cantinaGames.inRound && mx >= cx && mx <= cx + chipSize && my >= chipY && my <= chipY + chipSize) ? qtrue : qfalse;
			if ( s_hChips[c] > 0 ) {
				SCR_DrawPic( cx, chipY, chipSize, chipSize, s_hChips[c] );
				if ( chipHover ) {
					vec4_t hovBorder = { 1.0f, 0.9f, 0.2f, 1.0f };
					vec4_t empty = { 0, 0, 0, 0 };
					SCR_DrawRoundedGlassPanel( cx - 2.0f, chipY - 2.0f, chipSize + 4.0f, chipSize + 4.0f, chipSize * 0.5f, empty, hovBorder );
				}
			} else {
				vec4_t chipCols[5] = { { 0.15f, 0.40f, 0.85f, 0.9f }, { 0.10f, 0.70f, 0.35f, 0.9f }, { 0.85f, 0.20f, 0.20f, 0.9f }, { 0.10f, 0.12f, 0.18f, 0.9f }, { 0.55f, 0.20f, 0.85f, 0.9f } };
				const char *chipLabels[5] = { "+10", "+25", "+50", "+100", "+500" };
				SCR_DrawRoundedGlassPanel( cx, chipY, chipSize, chipSize, 4.0f, chipCols[c], chipHover ? yellowCol : feltBorder );
				SCR_DrawVirtualString( cx + 4.0f, chipY + 10.0f, 3.8f, chipLabels[c], whiteColor );
			}
		}

		// Clear Bet Button
		float clrX = winX + winW - 130.0f;
		float clrW = 105.0f;
		float clrH = 32.0f;
		qboolean clrHover = (!g_cantinaGames.inRound && mx >= clrX && mx <= clrX + clrW && my >= chipY && my <= chipY + clrH) ? qtrue : qfalse;
		qhandle_t hClr = clrHover ? s_hBtnClearHov : s_hBtnClear;
		if ( hClr > 0 ) {
			SCR_DrawPic( clrX, chipY + 1.0f, clrW, clrH, hClr );
		} else {
			vec4_t clrBg = { 0.25f, 0.10f, 0.10f, clrHover ? 0.9f : 0.6f };
			SCR_DrawRoundedGlassPanel( clrX, chipY, clrW, clrH, 4.0f, clrBg, feltBorder );
			SCR_DrawVirtualString( clrX + 16.0f, chipY + 8.0f, 3.8f, "CLEAR BET", whiteColor );
		}

		// Action Buttons Bar
		float btnY = winY + 318.0f;
		float btnW = 115.0f;
		float btnH = 34.0f;

		// DEAL
		qboolean dealHover = (!g_cantinaGames.inRound && mx >= winX + 25.0f && mx <= winX + 25.0f + btnW && my >= btnY && my <= btnY + btnH) ? qtrue : qfalse;
		qhandle_t hDeal = dealHover ? s_hBtnDealHov : s_hBtnDeal;
		{
			vec4_t tint = { 0.10f, 0.65f, 0.25f, 1.0f };
			const float en = ( g_cantinaGames.inRound ? 0.3f : (dealHover ? 1.0f : 0.8f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 25.0f, btnY, btnW, btnH, "DEAL [Spc]", 5.0f, (qboolean)( en > 0.95f ), tint );
		}

		// HIT
		qboolean hitHover = (g_cantinaGames.inRound && mx >= winX + 155.0f && mx <= winX + 155.0f + btnW && my >= btnY && my <= btnY + btnH) ? qtrue : qfalse;
		qhandle_t hHit = hitHover ? s_hBtnHitHov : s_hBtnHit;
		{
			vec4_t tint = { 0.15f, 0.40f, 0.80f, 1.0f };
			const float en = ( !g_cantinaGames.inRound ? 0.3f : (hitHover ? 1.0f : 0.8f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 155.0f, btnY, btnW, btnH, "HIT [1]", 5.0f, (qboolean)( en > 0.95f ), tint );
		}

		// STAND
		qboolean standHover = (g_cantinaGames.inRound && mx >= winX + 285.0f && mx <= winX + 285.0f + btnW && my >= btnY && my <= btnY + btnH) ? qtrue : qfalse;
		qhandle_t hStand = standHover ? s_hBtnStandHov : s_hBtnStand;
		{
			vec4_t tint = { 0.80f, 0.50f, 0.10f, 1.0f };
			const float en = ( !g_cantinaGames.inRound ? 0.3f : (standHover ? 1.0f : 0.8f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 285.0f, btnY, btnW, btnH, "STAND [2]", 5.0f, (qboolean)( en > 0.95f ), tint );
		}

		// DOUBLE
		qboolean dblHover = (g_cantinaGames.inRound && g_cantinaGames.playerCardCount == 2 && credits >= g_cantinaGames.currentBet && mx >= winX + 415.0f && mx <= winX + 415.0f + btnW && my >= btnY && my <= btnY + btnH) ? qtrue : qfalse;
		qhandle_t hDouble = dblHover ? s_hBtnDoubleHov : s_hBtnDouble;
		{
			vec4_t tint = { 0.55f, 0.20f, 0.75f, 1.0f };
			const float en = ( (!g_cantinaGames.inRound || g_cantinaGames.playerCardCount != 2) ? 0.3f : (dblHover ? 1.0f : 0.8f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 415.0f, btnY, btnW, btnH, "DOUBLE [3]", 4.4f, (qboolean)( en > 0.95f ), tint );
		}
		return;
	}

	// ========================================================
	// VIEW 2: STAR WARS PAZAAK 20 TABLE
	// ========================================================
	if ( g_cantinaGames.activeGame == 2 ) {
		float winW = 580.0f;
		float winH = 430.0f;
		float winX = 320.0f - winW * 0.5f;
		float winY = 240.0f - winH * 0.5f;

		vec4_t pzBg = { 0.04f, 0.09f, 0.18f, 0.97f };
		vec4_t pzBorder = { 0.25f, 0.75f, 0.95f, 0.95f };
		SCR_UIGameFrame( winX, winY, winW, winH, pzBg, pzBorder, "^5STAR WARS PAZAAK 20", 7.0f, 10.0f );
		SCR_DrawVirtualString( winX + 16.0f, winY + 10.0f, 4.2f, "< ^5[BACK]", cyanColor );

		// Tabs Bar: Tab 0: [ SOLO / MATCH ] | Tab 1: [ ONLINE PLAYERS & CHALLENGES ]
		float tabY = winY + 28.0f;
		float tabW = 240.0f;
		float tabH = 20.0f;

		// Tab 0 Button
		qboolean hTab0 = (qboolean)(mx >= winX + 25.0f && mx <= winX + 25.0f + tabW && my >= tabY && my <= tabY + tabH);
		vec4_t t0Bg = { 0.08f, 0.20f, 0.35f, (g_cantinaGames.pzTab == 0) ? 0.90f : (hTab0 ? 0.60f : 0.30f) };
		SCR_DrawRoundedGlassPanel( winX + 25.0f, tabY, tabW, tabH, 3.0f, t0Bg, (g_cantinaGames.pzTab == 0) ? pzBorder : NULL );
		const char *tab0Title = g_cantinaGames.pzInMatch ? va( "^2[ MATCH: vs %s ]", g_cantinaGames.pzOpponentName ) : "^2[ SOLO TABLE vs DROID ]";
		SCR_DrawVirtualString( winX + 35.0f, tabY + 4.0f, 4.2f, (g_cantinaGames.pzTab == 0) ? tab0Title : va( "^7%s", tab0Title ), whiteColor );

		// Tab 1 Button
		qboolean hTab1 = (qboolean)(mx >= winX + 285.0f && mx <= winX + 285.0f + tabW && my >= tabY && my <= tabY + tabH);
		vec4_t t1Bg = { 0.08f, 0.20f, 0.35f, (g_cantinaGames.pzTab == 1) ? 0.90f : (hTab1 ? 0.60f : 0.30f) };
		SCR_DrawRoundedGlassPanel( winX + 285.0f, tabY, tabW, tabH, 3.0f, t1Bg, (g_cantinaGames.pzTab == 1) ? pzBorder : NULL );
		SCR_DrawVirtualString( winX + 295.0f, tabY + 4.0f, 4.2f, (g_cantinaGames.pzTab == 1) ? "^3[ ONLINE PLAYERS & CHALLENGES ]" : "^7[ ONLINE PLAYERS & CHALLENGES ]", whiteColor );

		SCR_FillRect( winX + 15.0f, winY + 52.0f, winW - 30.0f, 1.0f, pzBorder );

		// ==================== TAB 1: ONLINE PLAYERS & CHALLENGES ====================
		if ( g_cantinaGames.pzTab == 1 ) {
			float contentX = winX + 25.0f;
			float contentW = winW - 50.0f;
			float curY = winY + 58.0f;

			// Incoming Challenge Banner
			if ( g_cantinaGames.pzPendingChallengerId >= 0 && cls.realtime < g_cantinaGames.pzPendingExpireTime ) {
				vec4_t invBg = { 0.05f, 0.22f, 0.12f, 0.95f };
				vec4_t invBorder = { 0.20f, 0.95f, 0.45f, 1.00f };
				SCR_DrawRoundedGlassPanel( contentX, curY, contentW, 30.0f, 5.0f, invBg, invBorder );
				SCR_DrawVirtualString( contentX + 10.0f, curY + 7.0f, 4.6f, va( "^3[CHALLENGE] ^5%s ^7challenged you to Pazaak for ^2%d Credits^7!", g_cantinaGames.pzPendingChallengerName, g_cantinaGames.pzPendingBet ), whiteColor );

				// ACCEPT Button
				float acX = contentX + contentW - 145.0f;
				float acY = curY + 4.0f;
				qboolean hAc = (qboolean)(mx >= acX && mx <= acX + 65.0f && my >= acY && my <= acY + 22.0f);
				vec4_t acBg = { 0.10f, 0.70f, 0.25f, hAc ? 1.0f : 0.80f };
				SCR_DrawRoundedGlassPanel( acX, acY, 65.0f, 22.0f, 3.0f, acBg, NULL );
				SCR_DrawVirtualString( acX + 10.0f, acY + 5.0f, 4.2f, "^7ACCEPT", whiteColor );

				// DECLINE Button
				float decX = contentX + contentW - 72.0f;
				float decY = curY + 4.0f;
				qboolean hDec = (qboolean)(mx >= decX && mx <= decX + 65.0f && my >= decY && my <= decY + 22.0f);
				vec4_t decBg = { 0.70f, 0.15f, 0.15f, hDec ? 1.0f : 0.80f };
				SCR_DrawRoundedGlassPanel( decX, decY, 65.0f, 22.0f, 3.0f, decBg, NULL );
				SCR_DrawVirtualString( decX + 8.0f, decY + 5.0f, 4.2f, "^7DECLINE", whiteColor );

				curY += 36.0f;
			}

			// Gather Online Players
			int onlineIds[MAX_CLIENTS];
			int onlineTotal = 0;
			int myClientNum = cl.snap.ps.clientNum;
			int csBase = SCR_GetPlayersCSBase();
			for ( int i = 0; i < MAX_CLIENTS; i++ ) {
				if ( i + csBase >= MAX_CONFIGSTRINGS ) break;
				if ( !cl.gameState.stringOffsets[ csBase + i ] ) continue;
				const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
				if ( !cInfo || !cInfo[0] ) continue;
				char nameBuf[64];
				Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );
				if ( !nameBuf[0] || i == myClientNum ) continue;
				onlineIds[onlineTotal++] = i;
			}

			int maxVisible = 7;
			int maxScroll = onlineTotal - maxVisible;
			if ( maxScroll < 0 ) maxScroll = 0;
			if ( g_cantinaGames.pzPlayerScroll > maxScroll ) g_cantinaGames.pzPlayerScroll = maxScroll;
			if ( g_cantinaGames.pzPlayerScroll < 0 ) g_cantinaGames.pzPlayerScroll = 0;

			SCR_DrawVirtualString( contentX, curY + 2.0f, 4.6f, va( "^3[ONLINE PLAYERS (%d)] ^7- Set Wager Below & Click Challenge [MWHEEL Scroll]", onlineTotal ), whiteColor );
			curY += 18.0f;

			float rowH = 26.0f;
			for ( int v = 0; v < maxVisible && (g_cantinaGames.pzPlayerScroll + v) < onlineTotal; v++ ) {
				int i = onlineIds[g_cantinaGames.pzPlayerScroll + v];
				const char *cInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ csBase + i ];
				char nameBuf[64];
				Q_strncpyz( nameBuf, Info_ValueForKey( cInfo, "n" ), sizeof( nameBuf ) );

				float rowY = curY + v * (rowH + 3.0f);
				vec4_t rBg = { 0.05f, 0.15f, 0.28f, (v % 2 == 0) ? 0.50f : 0.25f };
				SCR_FillRect( contentX, rowY, contentW, rowH, rBg );

				SCR_DrawVirtualString( contentX + 10.0f, rowY + 5.0f, 4.4f, va( "^7#%d ^5%s", i, nameBuf ), whiteColor );

				// [CHALLENGE] Button
				float chBtnX = contentX + contentW - 105.0f;
				float chBtnY = rowY + 2.0f;
				qboolean hCh = (qboolean)(mx >= chBtnX && mx <= chBtnX + 100.0f && my >= chBtnY && my <= chBtnY + 22.0f);
				vec4_t chBg = { 0.10f, 0.65f, 0.30f, hCh ? 1.0f : 0.80f };
				SCR_DrawRoundedGlassPanel( chBtnX, chBtnY, 100.0f, 22.0f, 3.0f, chBg, NULL );
				SCR_DrawVirtualString( chBtnX + 10.0f, chBtnY + 4.0f, 4.2f, va( "^7CHALLENGE (%d)", g_cantinaGames.pzBet ), whiteColor );
			}

			if ( onlineTotal == 0 ) {
				SCR_DrawVirtualString( contentX + 15.0f, curY + 30.0f, 4.8f, "^7No other players online right now. Switch to [SOLO TABLE] to play vs Droid!", whiteColor );
			}

			// Chip Selector for Challenge Wager
			float chipY = winY + winH - 52.0f;
			SCR_DrawVirtualString( contentX, chipY + 8.0f, 4.4f, va( "^7Wager: ^3%d CR", g_cantinaGames.pzBet ), yellowCol );
			float chipStartX = contentX + 140.0f;
			int chipVals[5] = { 10, 25, 50, 100, 500 };
			const char *chipLabels[5] = { "+10", "+25", "+50", "+100", "+500" };
			for ( int c = 0; c < 5; c++ ) {
				float cx = chipStartX + c * 52.0f;
				qboolean chipHover = (mx >= cx && mx <= cx + 46.0f && my >= chipY && my <= chipY + 26.0f) ? qtrue : qfalse;
				vec4_t cBg = { 0.10f, 0.40f, 0.70f, chipHover ? 0.90f : 0.60f };
				SCR_DrawRoundedGlassPanel( cx, chipY, 46.0f, 26.0f, 4.0f, cBg, chipHover ? yellowCol : pzBorder );
				SCR_DrawVirtualString( cx + 8.0f, chipY + 6.0f, 4.0f, chipLabels[c], whiteColor );
			}
			return;
		}

		// ==================== TAB 0: SOLO TABLE / ACTIVE MATCH ====================
		SCR_DrawVirtualString( winX + 20.0f, winY + 56.0f, 3.4f, "^7GET AS CLOSE TO 20 WITHOUT GOING OVER | FIRST TO 3 SETS WINS POT", whiteColor );
		SCR_DrawVirtualString( winX + winW - 150.0f, winY + 56.0f, 4.2f, va( "^7Bankroll: ^5%d CR", credits ), yellowCol );

		// Opponent Area (Top)
		float oppY = winY + 70.0f;
		SCR_DrawVirtualString( winX + 25.0f, oppY + 2.0f, 4.8f, va( "^7Opponent: ^5%s", g_cantinaGames.pzOpponentName ), cyanColor );
		
		// Opponent Set Win Orbs (3 sets to win match)
		char oppOrbs[32] = "";
		for ( int s = 0; s < 3; s++ ) {
			if ( s < g_cantinaGames.pzOppSetsWon ) Q_strcat( oppOrbs, sizeof(oppOrbs), "^1* " );
			else Q_strcat( oppOrbs, sizeof(oppOrbs), "^0- " );
		}
		SCR_DrawVirtualString( winX + 200.0f, oppY + 2.0f, 5.0f, va( "Sets: %s", oppOrbs ), whiteColor );
		SCR_DrawVirtualString( winX + winW - 130.0f, oppY + 2.0f, 5.2f, va( "Score: ^3%d^7%s", g_cantinaGames.pzOppScore, g_cantinaGames.pzOppStood ? " ^1[STOOD]" : "" ), yellowCol );

		// Opponent 3x3 Grid (9 slots)
		float cardW = 32.0f;
		float cardH = 38.0f;
		float oppGridX = winX + 25.0f;
		float oppGridY = oppY + 18.0f;
		for ( int i = 0; i < 9; i++ ) {
			float gx = oppGridX + (i % 9) * 36.0f;
			if ( i < g_cantinaGames.pzOppBoardCount ) {
				SCR_DrawPazaakMiniCard( gx, oppGridY, cardW, cardH, g_cantinaGames.pzOppBoard[i], qfalse, qfalse, qfalse );
			} else {
				vec4_t emptyBg = { 0.02f, 0.05f, 0.10f, 0.50f };
				vec4_t emptyBorder = { 0.15f, 0.30f, 0.50f, 0.40f };
				SCR_DrawRoundedGlassPanel( gx, oppGridY, cardW, cardH, 3.0f, emptyBg, emptyBorder );
			}
		}

		// Opponent Hand Cards (Hidden/Remaining)
		float oppHandX = winX + winW - 170.0f;
		for ( int h = 0; h < 4; h++ ) {
			float hx = oppHandX + h * 38.0f;
			qboolean isUsed = g_cantinaGames.pzOppHand[h].used;
			SCR_DrawPazaakMiniCard( hx, oppGridY, 32.0f, 38.0f, 0, qtrue, isUsed, qfalse );
		}

		// Table Middle: Match Status Message
		float midY = winY + 135.0f;
		vec4_t bannerBg = { 0.02f, 0.06f, 0.12f, 0.80f };
		SCR_DrawRoundedGlassPanel( winX + 20.0f, midY, winW - 40.0f, 26.0f, 6.0f, bannerBg, pzBorder );

		float msgW = SCR_GetStringWidth( g_cantinaGames.pzStatusMsg, 4.2f );
		SCR_DrawVirtualString( winX + (winW - msgW) * 0.5f, midY + 6.0f, 4.2f, g_cantinaGames.pzStatusMsg, whiteColor );

		// Player Area (Bottom Grid)
		float plyY = winY + 168.0f;
		SCR_DrawVirtualString( winX + 25.0f, plyY + 2.0f, 4.8f, "^7Your Board", cyanColor );

		// Player Set Win Orbs
		char plyOrbs[32] = "";
		for ( int s = 0; s < 3; s++ ) {
			if ( s < g_cantinaGames.pzPlayerSetsWon ) Q_strcat( plyOrbs, sizeof(plyOrbs), "^2* " );
			else Q_strcat( plyOrbs, sizeof(plyOrbs), "^0- " );
		}
		SCR_DrawVirtualString( winX + 200.0f, plyY + 2.0f, 5.0f, va( "Sets: %s", plyOrbs ), whiteColor );
		SCR_DrawVirtualString( winX + winW - 130.0f, plyY + 2.0f, 5.2f, va( "Score: ^2%d^7%s", g_cantinaGames.pzPlayerScore, g_cantinaGames.pzPlayerStood ? " ^3[STOOD]" : "" ), yellowCol );

		// Player 3x3 Grid
		float plyGridX = winX + 25.0f;
		float plyGridY = plyY + 18.0f;
		for ( int i = 0; i < 9; i++ ) {
			float gx = plyGridX + (i % 9) * 36.0f;
			if ( i < g_cantinaGames.pzPlayerBoardCount ) {
				SCR_DrawPazaakMiniCard( gx, plyGridY, cardW, cardH, g_cantinaGames.pzPlayerBoard[i], qfalse, qfalse, qfalse );
			} else {
				vec4_t emptyBg = { 0.02f, 0.05f, 0.10f, 0.50f };
				vec4_t emptyBorder = { 0.15f, 0.30f, 0.50f, 0.40f };
				SCR_DrawRoundedGlassPanel( gx, plyGridY, cardW, cardH, 3.0f, emptyBg, emptyBorder );
			}
		}

		// Player 4-Card Modifier Side-Deck Hand
		float handY = winY + 228.0f;
		SCR_DrawVirtualString( winX + 25.0f, handY + 2.0f, 4.4f, "^3Your Side-Deck Hand (Play 1 per turn):", yellowCol );

		float handStartX = winX + 25.0f;
		for ( int h = 0; h < 4; h++ ) {
			float hx = handStartX + h * 90.0f;
			float hy = handY + 18.0f;
			pzHandCard_t *c = &g_cantinaGames.pzPlayerHand[h];

			qboolean canPlay = (g_cantinaGames.pzInMatch && g_cantinaGames.pzIsPlayerTurn && !g_cantinaGames.pzPlayerStood && !g_cantinaGames.pzCardPlayedThisTurn && !c->used) ? qtrue : qfalse;
			qboolean cardHover = (canPlay && mx >= hx && mx <= hx + 80.0f && my >= hy && my <= hy + 46.0f) ? qtrue : qfalse;

			vec4_t cardPillBg = { (c->val < 0) ? 0.70f : 0.12f, (c->val < 0) ? 0.15f : 0.50f, (c->val < 0) ? 0.20f : 0.90f, c->used ? 0.35f : 0.95f };
			vec4_t cardPillBorder = { cardHover ? 1.0f : (c->isFlip ? 1.0f : 0.80f), cardHover ? 0.9f : (c->isFlip ? 0.85f : 0.85f), cardHover ? 0.2f : (c->isFlip ? 0.20f : 0.95f), 1.0f };
			SCR_DrawRoundedGlassPanel( hx, hy, 80.0f, 46.0f, 6.0f, cardPillBg, cardPillBorder );

			if ( !c->used ) {
				const char *signStr = (c->val > 0) ? "+" : "";
				char valBuf[16];
				Com_sprintf( valBuf, sizeof(valBuf), "%s%d", signStr, c->val );
				SCR_DrawVirtualString( hx + 12.0f, hy + 14.0f, 6.0f, valBuf, whiteColor );
				SCR_DrawVirtualString( hx + 55.0f, hy + 6.0f, 3.2f, va( "[%d]", h + 1 ), yellowCol );
				if ( c->isFlip ) {
					SCR_DrawVirtualString( hx + 50.0f, hy + 26.0f, 3.2f, "^3[FLIP]", cyanColor );
				}
			} else {
				SCR_DrawVirtualString( hx + 24.0f, hy + 16.0f, 4.0f, "^0USED", whiteColor );
			}
		}

		// Chip Wager Selector (3D Circular Chips)
		static qhandle_t s_hPzChips[5] = {0};
		static qhandle_t s_hBtnPzDeal = 0, s_hBtnPzDealHov = 0;
		static qhandle_t s_hBtnPzEnd = 0, s_hBtnPzEndHov = 0;
		static qhandle_t s_hBtnPzStand = 0, s_hBtnPzStandHov = 0;
		static qhandle_t s_hBtnPzForf = 0, s_hBtnPzForfHov = 0;

		if ( s_hPzChips[0] <= 0 && re && re->RegisterShaderNoMip ) {
			s_hPzChips[0] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_10" );
			s_hPzChips[1] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_25" );
			s_hPzChips[2] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_50" );
			s_hPzChips[3] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_100" );
			s_hPzChips[4] = re->RegisterShaderNoMip( "gfx/rpg_hud/chips/chip_500" );

			s_hBtnPzDeal = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_deal" );
			s_hBtnPzDealHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_deal_hover" );
			s_hBtnPzEnd = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_endturn" );
			s_hBtnPzEndHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_endturn_hover" );
			s_hBtnPzStand = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_stand" );
			s_hBtnPzStandHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_stand_hover" );
			s_hBtnPzForf = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_forfeit" );
			s_hBtnPzForfHov = re->RegisterShaderNoMip( "gfx/rpg_hud/buttons/btn_forfeit_hover" );
		}

		float pzChipY = winY + 300.0f;
		float pzChipSize = 32.0f;
		SCR_DrawVirtualString( winX + 25.0f, pzChipY + 8.0f, 4.2f, va( "^7Match Pot: ^3%d CR", g_cantinaGames.pzBet ), yellowCol );

		float pzChipStartX = winX + 175.0f;
		for ( int c = 0; c < 5; c++ ) {
			float cx = pzChipStartX + c * 46.0f;
			qboolean chipHover = (!g_cantinaGames.pzInMatch && mx >= cx && mx <= cx + pzChipSize && my >= pzChipY && my <= pzChipY + pzChipSize) ? qtrue : qfalse;
			if ( s_hPzChips[c] > 0 ) {
				SCR_DrawPic( cx, pzChipY, pzChipSize, pzChipSize, s_hPzChips[c] );
				if ( chipHover ) {
					vec4_t hovBorder = { 1.0f, 0.9f, 0.2f, 1.0f };
					vec4_t empty = { 0, 0, 0, 0 };
					SCR_DrawRoundedGlassPanel( cx - 2.0f, pzChipY - 2.0f, pzChipSize + 4.0f, pzChipSize + 4.0f, pzChipSize * 0.5f, empty, hovBorder );
				}
			} else {
				vec4_t chipCols[5] = { { 0.15f, 0.40f, 0.85f, 0.9f }, { 0.10f, 0.70f, 0.35f, 0.9f }, { 0.85f, 0.20f, 0.20f, 0.9f }, { 0.10f, 0.12f, 0.18f, 0.9f }, { 0.55f, 0.20f, 0.85f, 0.9f } };
				const char *chipLabels[5] = { "+10", "+25", "+50", "+100", "+500" };
				SCR_DrawRoundedGlassPanel( cx, pzChipY, 44.0f, 22.0f, 4.0f, chipCols[c], chipHover ? yellowCol : pzBorder );
				SCR_DrawVirtualString( cx + 6.0f, pzChipY + 5.0f, 3.6f, chipLabels[c], whiteColor );
			}
		}

		// Action Buttons (Bottom Bar)
		float pzBtnY = winY + 348.0f;
		float pzBtnW = 125.0f;
		float pzBtnH = 36.0f;

		// DEAL / NEW MATCH
		qboolean dealHover = (!g_cantinaGames.pzInMatch && mx >= winX + 25.0f && mx <= winX + 25.0f + pzBtnW && my >= pzBtnY && my <= pzBtnY + pzBtnH) ? qtrue : qfalse;
		qhandle_t hPzDeal = dealHover ? s_hBtnPzDealHov : s_hBtnPzDeal;
		{
			vec4_t tint = { 0.10f, 0.65f, 0.25f, 1.0f };
			const float en = ( g_cantinaGames.pzInMatch ? 0.30f : (dealHover ? 1.0f : 0.80f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 25.0f, pzBtnY, pzBtnW, pzBtnH, "DEAL MATCH", 4.8f, (qboolean)( en > 0.95f ), tint );
		}

		// END TURN
		qboolean endHover = (g_cantinaGames.pzInMatch && g_cantinaGames.pzIsPlayerTurn && !g_cantinaGames.pzPlayerStood && mx >= winX + 160.0f && mx <= winX + 160.0f + pzBtnW && my >= pzBtnY && my <= pzBtnY + pzBtnH) ? qtrue : qfalse;
		qhandle_t hPzEnd = endHover ? s_hBtnPzEndHov : s_hBtnPzEnd;
		{
			vec4_t tint = { 0.15f, 0.45f, 0.85f, 1.0f };
			const float en = ( (!g_cantinaGames.pzInMatch || !g_cantinaGames.pzIsPlayerTurn || g_cantinaGames.pzPlayerStood) ? 0.30f : (endHover ? 1.0f : 0.80f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 160.0f, pzBtnY, pzBtnW, pzBtnH, "END TURN [Spc]", 4.8f, (qboolean)( en > 0.95f ), tint );
		}

		// STAND
		qboolean standHover = (g_cantinaGames.pzInMatch && g_cantinaGames.pzIsPlayerTurn && !g_cantinaGames.pzPlayerStood && mx >= winX + 295.0f && mx <= winX + 295.0f + pzBtnW && my >= pzBtnY && my <= pzBtnY + pzBtnH) ? qtrue : qfalse;
		qhandle_t hPzStand = standHover ? s_hBtnPzStandHov : s_hBtnPzStand;
		{
			vec4_t tint = { 0.85f, 0.50f, 0.10f, 1.0f };
			const float en = ( (!g_cantinaGames.pzInMatch || !g_cantinaGames.pzIsPlayerTurn || g_cantinaGames.pzPlayerStood) ? 0.30f : (standHover ? 1.0f : 0.80f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 295.0f, pzBtnY, pzBtnW, pzBtnH, "STAND [S]", 4.8f, (qboolean)( en > 0.95f ), tint );
		}

		// FORFEIT / RESET
		qboolean forfHover = (g_cantinaGames.pzInMatch && mx >= winX + 430.0f && mx <= winX + 430.0f + pzBtnW && my >= pzBtnY && my <= pzBtnY + pzBtnH) ? qtrue : qfalse;
		qhandle_t hPzForf = forfHover ? s_hBtnPzForfHov : s_hBtnPzForf;
		{
			vec4_t tint = { 0.55f, 0.15f, 0.15f, 1.0f };
			const float en = ( !g_cantinaGames.pzInMatch ? 0.30f : (forfHover ? 1.0f : 0.70f) );
			if ( en < 0.5f ) { tint[0] *= 0.4f; tint[1] *= 0.4f; tint[2] *= 0.4f; }
			SCR_UIButton( winX + 430.0f, pzBtnY, pzBtnW, pzBtnH, "FORFEIT", 4.8f, (qboolean)( en > 0.95f ), tint );
		}
	}
}

void SCR_Games_f( void ) {
	g_cantinaGames.active = (!g_cantinaGames.active) ? qtrue : qfalse;
	g_cantinaGames.activeGame = 0;
}

void SCR_Blackjack_f( void ) {
	g_cantinaGames.active = qtrue;
	g_cantinaGames.activeGame = 1;
}

void SCR_Pazaak_f( void ) {
	g_cantinaGames.active = qtrue;
	g_cantinaGames.activeGame = 2;
	if ( !g_cantinaGames.pzInMatch ) {
		SCR_Pazaak_InitMatch();
	}
}

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {

	if ( cls.state != s_lastState ) {
		s_hAvatar = 0;
		s_hAvatarFrame = 0;
		s_hModalBg = 0;
		s_hInventoryBg = 0;
		s_hWantedBg = 0;
		s_hShopBg = 0;
		s_hQuestBg = 0;
		s_hAchBg = 0;
		s_hTopBg = 0;
		s_hAdvBg = 0;
		s_hBuyBtn = 0;
		s_hUseBtn = 0;
		s_hStatsCard = 0;
		s_hSellBtn = 0;
		s_hPotatoPic = 0;
		s_lastState = cls.state;

	}

	re->BeginFrame( stereoFrame );

	qboolean uiFullscreen = (qboolean)(cls.uiStarted && UIVM_IsFullscreen());

	if ( !cls.uiStarted ) {
		Com_DPrintf("draw screen without UI loaded\n");
		return;
	}

	// if the menu is going to cover the entire screen, we
	// don't need to render anything under it
	//actually, yes you do, unless you want clients to cycle out their reliable
	//commands from sitting in the menu. -rww
	if ( (cls.uiStarted && !uiFullscreen) || (!(cls.framecount&7) && cls.state == CA_ACTIVE) ) {
		switch( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			UIVM_SetActiveMenu( UIMENU_MAIN );
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			// refresh to update the time
			UIVM_Refresh( cls.realtime );
			UIVM_DrawConnectScreen( qfalse );
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			CL_CGameRendering( stereoFrame );

			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			// refresh to update the time
			UIVM_Refresh( cls.realtime );
			UIVM_DrawConnectScreen( qtrue );
			break;
		case CA_ACTIVE:
			CL_CGameRendering( stereoFrame );
			{
				extern keyGlobals_t kg;
				qboolean showScore = kg.keys[A_TAB].down;

				if ( !showScore ) {
					for ( int k = 0; k < MAX_KEYS; k++ ) {
						if ( kg.keys[k].down && kg.keys[k].binding && kg.keys[k].binding[0] ) {
							if ( strstr( kg.keys[k].binding, "+scores" ) || strstr( kg.keys[k].binding, "+score" ) ) {
								showScore = qtrue;
								break;
							}
						}
					}
				}

				if ( !showScore ) {
					SCR_DrawPartyOverlay();
					SCR_DrawLiveBPTracker();
					SCR_DrawRPGHUDOverlay();
					SCR_DrawDemoRecording();
					SCR_UIAnimated( 0, SCR_DrawLeaderboardOverlay );
					SCR_UIAnimated( 1, SCR_DrawStatsOverlay );
					SCR_DrawToastOverlay();
					SCR_DrawInspectOverlay();
					SCR_UIAnimated( 2, SCR_DrawBountyOverlay );
					SCR_UIAnimated( 3, SCR_DrawShopOverlay );
					SCR_UIAnimated( 4, SCR_DrawQuestOverlay );
					SCR_UIAnimated( 5, SCR_DrawInventoryOverlay );
					SCR_UIAnimated( 6, SCR_DrawCollectionOverlay );
					SCR_UIAnimated( 7, SCR_DrawAchievementsOverlay );
					SCR_UIAnimated( 8, SCR_DrawTopCreditsOverlay );
					SCR_UIAnimated( 9, SCR_DrawTopPotatoOverlay );
					SCR_UIAnimated( 10, SCR_DrawAdventureOverlay );
					void SCR_DrawRPGMenuOverlay( void );
					void SCR_DrawPartyStudioOverlay( void );
					void SCR_DrawAdminOverlay( void );
					SCR_DrawRPGMenuOverlay();
					SCR_UIAnimated( 14, SCR_DrawPartyStudioOverlay );
					SCR_UIAnimated( 15, SCR_DrawAdminOverlay );
					SCR_UIAnimated( 13, SCR_DrawCantinaGamesOverlay );

					// Draw dynamic virtual mouse cursor ONLY on active interactive menus (Shop, Inventory, Adventure, RPG Menu, Party Studio, Admin, Games)
					if ( g_rpgShop.active || g_rpgInventory.active || g_rpgCollection.active || g_rpgAdv.active || g_rpgMenu.active || g_rpgPartyStudio.active || g_rpgAdmin.active || g_cantinaGames.active ) {
						if ( s_hGlobalCursor <= 0 && re && re->RegisterShader ) {
							s_hGlobalCursor = re->RegisterShader( "gfx/rpg_hud/rpg_mouse" );
							if ( s_hGlobalCursor <= 0 ) s_hGlobalCursor = re->RegisterShader( "ui/assets/selectcursor.tga" );
							if ( s_hGlobalCursor <= 0 ) s_hGlobalCursor = re->RegisterShader( "ui/assets/cursor.tga" );
							if ( s_hGlobalCursor <= 0 ) s_hGlobalCursor = re->RegisterShader( "gfx/2d/cursor" );
						}
						if ( s_hGlobalCursor > 0 ) {
							SCR_DrawPic( (float)g_rpgMouseX, (float)g_rpgMouseY, 24.0f, 24.0f, s_hGlobalCursor );
						}
					}
				}

			}
			SCR_DrawPartyOverheadIcons();
			SCR_DrawHotPotatoOverheadIcon();
			break;


		}
	}

	// the menu draws next
	if ( Key_GetCatcher( ) & KEYCATCH_UI && cls.uiStarted ) {
		UIVM_Refresh( cls.realtime );
	}

	// console draws next
	Con_DrawConsole ();

	// debug graph can be drawn on top of anything
	if ( cl_debuggraph->integer || cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph ();
	}
}

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen( void ) {
	static int	recursive;

	if ( !scr_initialized ) {
		return;				// not initialized yet
	}

	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}
	recursive = 1;

	// If there is no VM, there are also no rendering commands issued. Stop the renderer in
	// that case.
	if( cls.uiStarted || com_dedicated->integer )
	{
		// if running in stereo, we need to draw the frame twice
		if ( cls.glconfig.stereoEnabled ) {
			SCR_DrawScreenField( STEREO_LEFT );
			SCR_DrawScreenField( STEREO_RIGHT );
		} else {
			SCR_DrawScreenField( STEREO_CENTER );
		}

		if ( com_speeds->integer ) {
			re->EndFrame( &time_frontend, &time_backend );
		} else {
			re->EndFrame( NULL, NULL );
		}
	}

	recursive = 0;
}

/*
==================
CL_ResetRPGOverlays

Resets all client-side RPG overlays and indicators.
==================
*/
/*
==================
CL_ResetShaderHandles

Resets all dynamically cached shader references.
==================
*/
void CL_ResetShaderHandles( void ) {
	s_uiFont = -1;
	s_hAvatar = 0;
	s_hAvatarFrame = 0;
	s_hModalBg = 0;
	s_hInventoryBg = 0;
	s_hWantedBg = 0;
	s_hShopBg = 0;
	s_hQuestBg = 0;
	s_hAchBg = 0;
	s_hTopBg = 0;
	s_hAdvBg = 0;
	s_hPotatoPic = 0;
	s_hBuyBtn = 0;
	s_hSellBtn = 0;
	s_hUseBtn = 0;
	s_hStatsCard = 0;
	s_hSettingsBg = 0;
	s_hSettingsBtnNormal = 0;
	s_hSettingsBtnHover = 0;
	s_hSettingsSliderTrack = 0;
	s_hSettingsSliderThumb = 0;
	s_hGlobalCursor = 0;
}

/*
==================
CL_ResetRPGOverlays

Resets all client-side RPG overlays and indicators.
==================
*/
void CL_ResetRPGOverlays( void ) {
	CL_ResetShaderHandles();
	g_hotPotatoHolder = -1;
	g_rpgAdv.active = qfalse;
	g_rpgShop.active = qfalse;
	g_rpgQuest.active = qfalse;
	g_rpgInventory.active = qfalse;
	g_rpgCollection.active = qfalse;
	g_rpgAch.active = qfalse;
	g_rpgTopCredits.active = qfalse;
	g_rpgTopPotato.active = qfalse;
	g_rpgStats.active = qfalse;
	g_rpgBounty.active = qfalse;
	g_rpgMenu.active = qfalse;
	g_rpgPartyStudio.active = qfalse;
	g_rpgAdmin.active = qfalse;
	g_rpgSettings.active = qfalse;
	g_cantinaGames.active = qfalse;
	g_rpgStats.xp = 0; // Just in case, reset stats too
	Cvar_Set( "cg_drawShop", "0" );
	Cvar_Set( "cg_drawQuest", "0" );
	Cvar_Set( "cg_drawInventory", "0" );
	Cvar_Set( "cg_drawAch", "0" );
	Cvar_Set( "cg_drawTopCredits", "0" );
	Cvar_Set( "cg_drawTopPotato", "0" );
	Cvar_Set( "cg_drawAdv", "0" );
	Cvar_Set( "cg_drawLeaderboard", "0" );
	Cvar_Set( "cg_drawStats", "0" );
	Cvar_Set( "cg_drawBounty", "0" );
}
