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


/*
================
SCR_FillRect

Coordinates are 640*480 virtual values
=================
*/
void SCR_FillRect( float x, float y, float width, float height, const float *color ) {
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
	re->DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}



float SCR_GetCharWidthFactor( int ch ) {
	return 0.50f;
}

float SCR_GetStringWidth( const char *str, float charSize ) {
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
		Com_Printf( "^3Usage: ^7rpg_hud_style <classic | bottom | style2 | style3 | 0 | 1 | 2 | 3>\n" );
		return;
	}
	const char *style = Cmd_Argv( 1 );
	if ( Cmd_Argc() >= 3 && !Q_stricmp( style, "style" ) ) {
		style = Cmd_Argv( 2 );
	}
	if ( !Q_stricmp( style, "bottom" ) || !Q_stricmp( style, "1" ) ) {
		Cvar_Set( "cg_rpg_style", "1" );
		Com_Printf( "^2RPG HUD style set to BOTTOM SLEEK BAR (Style 1)\n" );
	} else if ( !Q_stricmp( style, "style2" ) || !Q_stricmp( style, "2" ) ) {
		Cvar_Set( "cg_rpg_style", "2" );
		Com_Printf( "^2RPG HUD style set to STYLE V2 (Style 2)\n" );
	} else if ( !Q_stricmp( style, "style3" ) || !Q_stricmp( style, "3" ) ) {
		Cvar_Set( "cg_rpg_style", "3" );
		Com_Printf( "^2RPG HUD style set to STYLE V3 (Style 3)\n" );
	} else {
		Cvar_Set( "cg_rpg_style", "0" );
		Com_Printf( "^2RPG HUD style set to CLASSIC PANEL V1 (Style 0)\n" );
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
	Cmd_AddCommand( "menu", SCR_RPGMenu_f, "Toggle Master RPG Hub menu overlay" );
	Cmd_AddCommand( "settings", SCR_RPGMenu_f, "Toggle Master RPG Hub menu overlay" );
	Cmd_AddCommand( "partymenu", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "party", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "partystudio", SCR_PartyMenu_f, "Toggle Party Management Studio overlay" );
	Cmd_AddCommand( "adminmenu", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );
	Cmd_AddCommand( "admin", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );
	Cmd_AddCommand( "adminpanel", SCR_AdminMenu_f, "Toggle Admin Control Panel overlay" );

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
	re->SetColor( color );

	float renderH = (charSize < 7.0f) ? (charSize * 1.35f) : charSize;
	float advance = renderH * 0.50f;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			Com_Memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
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
static qhandle_t s_hBox = 0;
static qhandle_t s_hBoxFillV1 = 0;
static qhandle_t s_hBoxV2 = 0;
static qhandle_t s_hBoxFillV2 = 0;
static qhandle_t s_hBoxV3 = 0;
static qhandle_t s_hBoxFillV3 = 0;
static qhandle_t s_hBarBg = 0;
static qhandle_t s_hBarFill = 0;
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

void SCR_DrawRPGHUDOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Register HD TGA Shaders dynamically whenever active
	if ( s_hBox <= 0 && re && re->RegisterShader ) {
		s_hBox         = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
		s_hBoxFillV1   = re->RegisterShader( "gfx/rpg_hud/panel_bg_fill_v1" );
		s_hBoxV2       = re->RegisterShader( "gfx/rpg_hud/panel_bg_v2" );
		s_hBoxFillV2   = re->RegisterShader( "gfx/rpg_hud/panel_bg_fill_v2" );
		s_hBoxV3       = re->RegisterShader( "gfx/rpg_hud/panel_bg_v3" );
		s_hBoxFillV3   = re->RegisterShader( "gfx/rpg_hud/panel_bg_fill_v3" );
		s_hBarBg       = re->RegisterShader( "gfx/rpg_hud/bar_bg" );
		s_hBarFill     = re->RegisterShader( "gfx/rpg_hud/bar_fill" );
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
	if ( style < 0 || style > 3 ) style = 0;

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
	if ( style == 1 ) {
		panelW = (maxStrW + 115.0f);
		if ( panelW < 180.0f ) panelW = 180.0f;
		panelH = 32.0f;
	} else {
		panelW = 175.0f;
		panelH = 35.0f;
	}

	// Preset position defaults
	float defaultX = (style == 1) ? (320.0f - panelW * 0.5f) : 14.0f;
	float defaultY = (style == 1) ? 428.0f : 14.0f;

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
	// STYLE 1: BOTTOM SLEEK BAR (Expanded Floating Elements)
	// ========================================================
	if ( style == 1 ) {
		float avatarX = panelX + 3.0f;
		float avatarY = panelY + 1.0f;
		float avatarSize = 26.0f;

		if ( s_hAvatar && s_hAvatarFrame ) {
			SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatarFrame );
			SCR_DrawPic( avatarX + 1.0f, avatarY + 1.0f, avatarSize - 2.0f, avatarSize - 2.0f, s_hAvatar );
		} else if ( s_hAvatar ) {
			SCR_DrawPic( avatarX + 1.0f, avatarY + 1.0f, avatarSize - 2.0f, avatarSize - 2.0f, s_hAvatar );
		} else {
			float cx = avatarX + avatarSize * 0.5f;
			float cy = avatarY + avatarSize * 0.5f;
			SCR_DrawJediVectorEmblem( cx, cy, avatarSize * 0.5f );
		}

		// Player Info Column next to Circular Avatar
		float textX = avatarX + avatarSize + 8.0f;

		// Line 1: Player Name + Level Badge
		char nameLvlStr[128];
		Com_sprintf( nameLvlStr, sizeof(nameLvlStr), "^7%s ^3Lv %d", playerName, level );
		SCR_DrawVirtualString( textX, panelY + 2.0f, 5.2f, nameLvlStr, whiteColor );

		// Line 2: Rank Title, Force Rating ELO & Kills/Deaths/Wins/Losses
		char rankStr[128];
		Com_sprintf( rankStr, sizeof(rankStr), "^3%s ^7|^2 %d  ^2%dK ^1%dD  ^2%dW ^1%dL", rankTitle, fr, g_rpgStats.kills, g_rpgStats.deaths, g_rpgStats.wins, g_rpgStats.losses );
		SCR_DrawVirtualString( textX, panelY + 13.0f, 3.8f, rankStr, whiteColor );

		// Line 3: Sleek Horizontal XP Bar (TGA Background)
		float barX = textX;
		float barY = panelY + 25.0f;
		float barW = panelX + panelW - barX - 4.0f;
		float barH = 8.0f;

		if ( s_hBarBg ) {
			SCR_DrawPic( barX, barY, barW, barH, s_hBarBg );
		} else {
			vec4_t barBorder = { 0.00f, 0.60f, 0.95f, 0.40f };
			vec4_t barBg     = { 0.02f, 0.04f, 0.08f, 0.40f };
			SCR_DrawMBIICapsule( barX, barY, barW, barH, barBg, barBorder );
		}

		float fillX = barX;
		float fillY = barY;
		float maxFillW = barW;
		float fillW = maxFillW * xpRatio;
		float fillH = barH;

		if ( fillW > 0.0f ) {
			if ( s_hBarFill ) {
				SCR_DrawPic( fillX, fillY, fillW, fillH, s_hBarFill );
			} else {
				vec4_t cyanFill = { 0.00f, 0.70f, 0.95f, 0.95f };
				SCR_DrawMBIICapsule( fillX, fillY, fillW, fillH, cyanFill, NULL );
			}
		}

		// XP Numeric Readout Overlay
		int totalXP = (g_rpgStats.xp > 0) ? g_rpgStats.xp : (int)s_visualXP;
		int totalXPMax = (g_rpgStats.xp > 0) ? (g_rpgStats.xp + (xpMax - relativeXP)) : xpMax;
		char xpText[64];
		Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", totalXP, totalXPMax );
		float textWidthPixels = SCR_GetStringWidth( xpText, 3.8f );
		float xpTextX = barX + barW - textWidthPixels - 3.0f;
		if ( xpTextX < barX + 3.0f ) xpTextX = barX + 3.0f;
		SCR_DrawVirtualString( xpTextX, barY - 10.0f, 3.8f, xpText, whiteColor );
		return;
	}

	// ========================================================
	// STYLE 0/2/3: GLASS PANEL CARD (V1, V2, V3 Options)
	// ========================================================
	qhandle_t drawOutline = 0;
	qhandle_t drawFill = 0;
	float fillXRatio = 0.20f;
	float fillWRatio = 0.74f;
	float fillYRatio = 0.52f;
	float fillHRatio = 0.18f;

	if ( style == 2 ) {
		drawOutline = s_hBoxV2;
		drawFill = s_hBoxFillV2;
		fillXRatio = 0.21f;
		fillWRatio = 0.74f;
		fillYRatio = 0.55f;
		fillHRatio = 0.20f;
	} else if ( style == 3 ) {
		drawOutline = s_hBoxV3;
		drawFill = s_hBoxFillV3;
		fillXRatio = 0.19f;
		fillWRatio = 0.74f;
		fillYRatio = 0.55f;
		fillHRatio = 0.14f;
	} else {
		drawOutline = s_hBox;
		drawFill = s_hBoxFillV1;
		fillXRatio = 0.20f;
		fillWRatio = 0.74f;
		fillYRatio = 0.52f;
		fillHRatio = 0.18f;
	}

	if ( drawOutline > 0 ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, drawOutline );
	} else {
		vec4_t bgColor     = { 0.02f, 0.05f, 0.10f, 0.20f };
		vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.40f };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, bgColor, borderColor );
	}

	// Avatar (Fitted inside circular frame)
	float avatarX = panelX + panelW * 0.04f;
	float avatarY = panelY + panelH * 0.14f;
	float avatarSize = panelH * 0.72f;

	if ( style == 3 ) {
		avatarX = panelX + 5.0f;
		avatarSize = 20.0f;
		avatarY = panelY + 7.5f;
	}

	if ( s_hAvatar ) {
		if ( drawOutline <= 0 && s_hAvatarFrame > 0 ) {
			SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatarFrame );
			SCR_DrawPic( avatarX + 1.5f, avatarY + 1.5f, avatarSize - 3.0f, avatarSize - 3.0f, s_hAvatar );
		} else {
			SCR_DrawPic( avatarX + 1.5f, avatarY + 1.5f, avatarSize - 3.0f, avatarSize - 3.0f, s_hAvatar );
		}
	} else {
		float cx = avatarX + avatarSize * 0.5f;
		float cy = avatarY + avatarSize * 0.5f;
		SCR_DrawJediVectorEmblem( cx, cy, avatarSize * 0.5f );
	}

	// Right Content Column
	float textX = panelX + panelW * 0.20f;

	// Line 1: Player Name & Level Badge
	char nameStr[128];
	Com_sprintf( nameStr, sizeof(nameStr), "^7%s ^3Lv %d", playerName, level );
	
	float nameY;
	if ( style == 0 ) {
		nameY = panelY + 8.5f;
	} else if ( style == 2 ) {
		nameY = panelY + 4.0f;
	} else {
		nameY = panelY + 5.0f;
	}
	SCR_DrawVirtualString( textX, nameY, 3.8f, nameStr, whiteColor );

	// Line 2: Rank Title, Force Rating ELO & K/D/W/L
	char rankStr[128];
	Com_sprintf( rankStr, sizeof(rankStr), "^3%s ^7|^2 %d ^7[^2%dK^7/^1%dD ^2%dW^7/^1%dL^7]", rankTitle, fr, g_rpgStats.kills, g_rpgStats.deaths, g_rpgStats.wins, g_rpgStats.losses );
	
	if ( style == 0 ) {
		float rankW = SCR_GetStringWidth( rankStr, 3.0f );
		float rankX = panelX + panelW - rankW - 4.0f;
		if ( rankX < textX ) rankX = textX;
		SCR_DrawVirtualString( rankX, panelY + 13.0f, 3.0f, rankStr, whiteColor );
	} else if ( style == 2 ) {
		SCR_DrawVirtualString( textX, panelY + 12.0f, 3.0f, rankStr, whiteColor );
	} else {
		SCR_DrawVirtualString( textX, panelY + 13.0f, 3.0f, rankStr, whiteColor );
	}

	// Line 3: XP Bar (drawn dynamically inside baked-in card slots using style-specific filler textures)
	float fX = panelX + panelW * fillXRatio;
	float fY = panelY + panelH * fillYRatio;
	float fW = panelW * fillWRatio;
	float fH = panelH * fillHRatio;

	float fillW = fW * xpRatio;
	if ( fillW > 0.0f && drawFill > 0 ) {
		SCR_DrawPic( fX, fY, fillW, fH, drawFill );
	}

	int totalXP = (g_rpgStats.xp > 0) ? g_rpgStats.xp : (int)s_visualXP;
	int totalXPMax = (g_rpgStats.xp > 0) ? (g_rpgStats.xp + (xpMax - relativeXP)) : xpMax;

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", totalXP, totalXPMax );

	float textWidthPixels = SCR_GetStringWidth( xpText, 3.0f );
	float xpTextX = fX + fW - textWidthPixels - 4.0f;
	if ( xpTextX < fX + 3.0f ) xpTextX = fX + 3.0f;
	float xpTextY = fY + (fH - 3.0f) * 0.5f;
	SCR_DrawVirtualString( xpTextX, xpTextY, 3.0f, xpText, whiteColor );
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
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	if ( !cg_drawLeaderboard || !cg_drawLeaderboard->integer ) {
		return;
	}

	// Modal Window Dimensions (Vertical 290x400 centered card using leaderboard_bg)
	float winW = 290.0f;
	float winH = 400.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	static qhandle_t s_hLeaderboardBg = 0;
	if ( s_hLeaderboardBg <= 0 && re && re->RegisterShader ) {
		s_hLeaderboardBg = re->RegisterShader( "gfx/rpg_hud/top_bg" );
	}

	if ( s_hLeaderboardBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hLeaderboardBg );
	} else {
		vec4_t bgColor     = { 0.03f, 0.06f, 0.12f, 0.88f };
		vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 26.0f, 3.0f, headerBg, NULL );
	}

	// Title (removed yellow virtual title overlap as requested)
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// Close Button instruction (nudged 3 ticks right, 1 tick down)
	SCR_DrawVirtualString( winX + winW - 37.0f, winY + 15.0f, 5.0f, "^1[ESC]", yellowCol );

	// Column Headers Divider line (right above the blue line on top UI)
	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.70f };
	float colY = winY + 40.0f;
	SCR_FillRect( winX + 10.0f, colY + 16.0f, winW - 20.0f, 1.0f, divColor );

	// Column Headers (moved 6 ticks to the right, 8 ticks down): # | PLAYER NAME | LV | TITLE | ELO
	SCR_DrawVirtualString( winX + 26.0f, colY, 4.8f, "^5#", whiteColor );
	SCR_DrawVirtualString( winX + 46.0f, colY, 4.8f, "^5PLAYER NAME", whiteColor );
	SCR_DrawVirtualString( winX + 141.0f, colY, 4.8f, "^5LV", whiteColor );
	SCR_DrawVirtualString( winX + 171.0f, colY, 4.8f, "^5TITLE", whiteColor );
	SCR_DrawVirtualString( winX + winW - 84.0f, colY, 4.8f, "^5ELO", whiteColor );

	// Render Rows (moved 8 ticks down from previous 60.0f)
	float rowStartY = winY + 68.0f;
	float rowHeight = 23.0f;

	for ( int i = 0; i < 10; i++ ) {
		float currentY = rowStartY + (i * rowHeight);

		// Alternating row background highlight
		if ( i % 2 == 0 ) {
			vec4_t rowBg = { 0.05f, 0.12f, 0.25f, 0.35f };
			SCR_FillRect( winX + 10.0f, currentY, winW - 20.0f, rowHeight - 2.0f, rowBg );
		}

		if ( i < g_topLeaderboardCount ) {
			topLeaderboardEntry_t *e = &g_topLeaderboard[i];

			// Rank position # (moved 6 ticks right)
			char numStr[16];
			Com_sprintf( numStr, sizeof(numStr), (i < 3) ? "^3#%d" : "^7#%d", i + 1 );
			SCR_DrawVirtualString( winX + 26.0f, currentY + 2.0f, 4.8f, numStr, whiteColor );

			// Player Name (moved 6 ticks right - un-truncated name printing)
			char pNameStr[64];
			Com_sprintf( pNameStr, sizeof(pNameStr), "^7%s", e->displayName );
			SCR_DrawVirtualString( winX + 46.0f, currentY + 2.0f, 4.8f, pNameStr, whiteColor );

			// Level (moved 6 ticks right)
			char lvlStr[16];
			Com_sprintf( lvlStr, sizeof(lvlStr), "^3%d", e->level );
			SCR_DrawVirtualString( winX + 141.0f, currentY + 2.0f, 4.8f, lvlStr, whiteColor );

			// Rank title (moved 6 ticks right)
			char titleStr[32];
			Com_sprintf( titleStr, sizeof(titleStr), "^3%.14s", e->rankTitle );
			SCR_DrawVirtualString( winX + 171.0f, currentY + 2.0f, 4.4f, titleStr, whiteColor );

			// FR ELO (moved 6 ticks right)
			char eloStr[16];
			Com_sprintf( eloStr, sizeof(eloStr), "^2%d", e->fr );
			SCR_DrawVirtualString( winX + winW - 84.0f, currentY + 2.0f, 4.8f, eloStr, whiteColor );
		}
	}

	SCR_DrawVirtualString( winX + 26.0f, winY + winH - 13.0f, 4.0f, "^7Press ^1[ESC]^7 to close Duelist Leaderboard", whiteColor );
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

	// Modal Window Dimensions (Vertical stats_card layout - Compacted 290x400)
	float winW = 290.0f;
	float winH = 400.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.85f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	if ( s_hStatsCard <= 0 && re && re->RegisterShader ) {
		s_hStatsCard = re->RegisterShader( "gfx/rpg_hud/stats_card" );
	}

	if ( s_hStatsCard > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hStatsCard );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 22.0f, 3.0f, headerBg, NULL );

		// Title (Only drawn as a fallback when texture is not loaded)
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, winY + 8.0f, 4.8f, "^3STATISTICS", yellowCol );
	}

	// Close Button instruction
	SCR_DrawVirtualString( winX + winW - 35.0f, winY + 11.0f, 4.4f, "^1[ESC]", yellowCol );

	// TOP HALF - Profile Picture & Profile details (Scaled to fits inside frame)
	float avatarX = winX + 27.0f;
	float avatarY = winY + 59.0f;
	float avatarSize = 72.0f;

	qhandle_t hAv = s_hAvatar;
	if ( !hAv ) hAv = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
	if ( hAv ) {
		SCR_DrawPic( avatarX + 2.0f, avatarY + 2.0f, avatarSize - 4.0f, avatarSize - 4.0f, hAv );
	}
	// Only draw avatar frame if stats_card is NOT loaded
	if ( s_hStatsCard <= 0 && s_hAvatarFrame > 0 ) {
		SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatarFrame );
	}

	float profileX = winX + 108.0f;
	float profileY = winY + 59.0f;

	// Line 1: Player Name
	char dName[128];
	Com_sprintf( dName, sizeof(dName), "^7%s", g_rpgStats.name[0] ? g_rpgStats.name : "Player" );
	SCR_DrawVirtualString( profileX, profileY + 4.0f, 5.2f, dName, whiteColor );

	// Line 2: Rank Title
	char rTitle[128];
	Com_sprintf( rTitle, sizeof(rTitle), "^3%s", g_rpgStats.rankTitle[0] ? g_rpgStats.rankTitle : "Padawan" );
	SCR_DrawVirtualString( profileX, profileY + 18.0f, 4.8f, rTitle, whiteColor );

	// Line 3: Level & ELO
	char lvlElo[128];
	Com_sprintf( lvlElo, sizeof(lvlElo), "^5Lv %d  ^7|  ^2%d ELO", g_rpgStats.level, g_rpgStats.elo );
	SCR_DrawVirtualString( profileX, profileY + 32.0f, 4.6f, lvlElo, whiteColor );

	// Line 4: XP Progress
	char xpStr[128];
	Com_sprintf( xpStr, sizeof(xpStr), "^5XP: ^2%d", g_rpgStats.xp );
	SCR_DrawVirtualString( profileX, profileY + 46.0f, 4.6f, xpStr, whiteColor );

	// Divider line between columns (fallback only)
	if ( s_hStatsCard <= 0 ) {
		vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
		SCR_FillRect( winX + 105.0f, winY + 34.0f, 1.0f, 85.0f, divColor );
	}

	// BOTTOM HALF - Statistics Table inside the container box
	float bodyX = winX + 23.0f;
	float bodyY = winY + 141.0f;
	float bodyW = 244.0f;
	
	float statsX = bodyX + 15.0f;
	float valX = winX + 175.0f;
	float statsY = bodyY + 18.0f;
	float spacingY = 25.5f;

	// Row 1: Credits Balance
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Credits Balance:", whiteColor );
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^3%d CR", g_rpgStats.credits ), whiteColor );
	statsY += spacingY;

	// Row 2: Wins / Losses
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Wins / Losses:", whiteColor );
	float wlRatio = g_rpgStats.losses > 0 ? (float)g_rpgStats.wins / (float)g_rpgStats.losses : (float)g_rpgStats.wins;
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^2%d^7/^1%d ^5(%.2f)", g_rpgStats.wins, g_rpgStats.losses, wlRatio ), whiteColor );
	statsY += spacingY;

	// Row 3: Kills / Deaths
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Kills / Deaths:", whiteColor );
	float kdRatio = g_rpgStats.deaths > 0 ? (float)g_rpgStats.kills / (float)g_rpgStats.deaths : (float)g_rpgStats.kills;
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^2%d^7/^1%d ^5(%.2f)", g_rpgStats.kills, g_rpgStats.deaths, kdRatio ), whiteColor );
	statsY += spacingY;

	// Row 4: Highest Streak
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Highest Streak:", whiteColor );
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^3%d", g_rpgStats.highestStreak ), whiteColor );
	statsY += spacingY;

	// Row 5: Favorite Weapon
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Fav Weapon:", whiteColor );
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^7%s", g_rpgStats.favWeapon[0] ? g_rpgStats.favWeapon : "Lightsaber" ), whiteColor );
	statsY += spacingY;

	// Row 6: Trivia Wins
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Trivia Wins:", whiteColor );
	SCR_DrawVirtualString( valX, statsY, 4.8f, va( "^7%d", g_rpgStats.triviaWins ), whiteColor );
	statsY += spacingY;

	// Row 7: Main Rival
	SCR_DrawVirtualString( statsX, statsY, 4.8f, "^5Main Rival:", whiteColor );
	char rivalStr[128];
	if ( g_rpgStats.topRivalCount > 0 ) {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^1%s ^5(%d)", g_rpgStats.topRivalName, g_rpgStats.topRivalCount );
	} else {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^7None" );
	}
	SCR_DrawVirtualString( valX, statsY, 4.8f, rivalStr, whiteColor );

	// Footer instruction
	SCR_DrawVirtualString( winX + 21.0f, winY + 386.0f, 3.8f, "^7Press ^3F8^7, ^3ESC^7, or type ^3!stats^7 to close", whiteColor );
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

	// Calculate alpha (fade-in / fade-out)
	float alpha = 1.0f;
	if ( elapsed < TOAST_FADEIN_MS ) {
		alpha = (float)elapsed / (float)TOAST_FADEIN_MS;
	} else if ( elapsed > TOAST_DURATION_MS - TOAST_FADEOUT_MS ) {
		alpha = (float)(TOAST_DURATION_MS - elapsed) / (float)TOAST_FADEOUT_MS;
	}
	if ( alpha < 0.0f ) alpha = 0.0f;
	if ( alpha > 1.0f ) alpha = 1.0f;

	const float panelW = 150.0f;
	const float panelH = 46.0f;
	const float panelX = 640.0f - panelW - 14.0f;  // Top-Right corner
	const float panelY = 125.0f;                   // Under the map

	vec4_t whiteA = { 1.0f, 1.0f, 1.0f, alpha };

	// Panel background (dark navy glass)
	vec4_t bgColor   = { 0.04f, 0.07f, 0.14f, 0.88f * alpha };
	re->SetColor( bgColor );
	re->DrawStretchPic( panelX, panelY, panelW, panelH, 0, 0, 0, 0, cls.whiteShader );
	re->SetColor( NULL );

	// Outer border
	vec4_t borderColor = { 0.20f, 0.50f, 0.80f, 0.55f * alpha };
	SCR_FillRect( panelX,              panelY,              panelW, 1.0f,   borderColor );
	SCR_FillRect( panelX,              panelY + panelH - 1, panelW, 1.0f,   borderColor );
	SCR_FillRect( panelX,              panelY,              1.0f,   panelH, borderColor );
	SCR_FillRect( panelX + panelW - 1, panelY,              1.0f,   panelH, borderColor );

	// Left accent bar (3px wide): green for win, red for loss
	vec4_t accentColor;
	if ( g_rpgToast.isWin ) {
		accentColor[0] = 0.20f;
		accentColor[1] = 1.00f;
		accentColor[2] = 0.40f;
		accentColor[3] = alpha;
	} else {
		accentColor[0] = 1.00f;
		accentColor[1] = 0.20f;
		accentColor[2] = 0.20f;
		accentColor[3] = alpha;
	}
	SCR_FillRect( panelX, panelY, 3.0f, panelH, accentColor );

	const float textX = panelX + 8.0f;

	// Row 1: Title (VICTORY! / DEFEAT.)
	const char *titleStr;
	if ( g_rpgToast.isWin ) {
		titleStr = "^2VICTORY!";
	} else {
		titleStr = "^1DEFEAT.";
	}
	SCR_DrawVirtualString( textX, panelY + 4.0f, 5.2f, titleStr, whiteA );

	// Row 2: vs. Opponent name
	char vsStr[80];
	Com_sprintf( vsStr, sizeof( vsStr ), "^7vs. ^5%.24s", g_rpgToast.opponentName );
	SCR_DrawVirtualString( textX, panelY + 15.0f, 4.0f, vsStr, whiteA );

	// Row 3: Stats row
	char statsStr[128];
	if ( g_rpgToast.isWin ) {
		if ( g_rpgToast.bp > 0 ) {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP ^7(^2%d HP ^7| ^5%d BP remaining^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp, g_rpgToast.health, g_rpgToast.bp );
		} else if ( g_rpgToast.health > 0 ) {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP ^7(^2%d HP^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp, g_rpgToast.health );
		} else {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp );
		}

	} else {
		if ( g_rpgToast.bp > 0 ) {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^1%d ^7| ^6+%d CR ^7(^1%d HP ^7| ^5%d BP remaining^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.health, g_rpgToast.bp );
		} else if ( g_rpgToast.health > 0 ) {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^1%d ^7| ^6+%d CR ^7(^1%d HP^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.health );
		} else {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^1%d ^7| ^6+%d CR", g_rpgToast.eloDelta, g_rpgToast.credits );
		}
	}
	SCR_DrawVirtualString( textX, panelY + 26.0f, 4.0f, statsStr, whiteA );


	// Progress bar (shrinks left to right as duration elapses)
	float barW    = panelW - 4.0f;
	float barFill = barW * ( 1.0f - (float)elapsed / (float)TOAST_DURATION_MS );
	vec4_t barBg   = { 0.10f, 0.10f, 0.18f, 0.70f * alpha };
	SCR_FillRect( panelX + 2.0f, panelY + panelH - 3.0f, barW,    2.0f, barBg );
	if ( barFill > 0.0f ) {
		SCR_FillRect( panelX + 2.0f, panelY + panelH - 3.0f, barFill, 2.0f, accentColor );
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

void SCR_DrawInspectOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	// Check crosshair target every frame
	int crosshairNum = -1;
	if ( cls.cgameStarted ) {
		crosshairNum = CGVM_CrosshairPlayer();
	}

	// Poll server periodically if looking at a valid player
	if ( crosshairNum >= 0 && crosshairNum < 64 ) {
		if ( cls.realtime - s_inspectLastPollMs >= INSPECT_POLL_MS ) {
			s_inspectLastPollMs = cls.realtime;
			CL_AddReliableCommand( va( "inspect %d", crosshairNum ), qfalse );
		}
	} else {
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

	// Modal Window Dimensions (Vertical 300x400 centered card using wanted_bg.tga)
	float winW = 300.0f;
	float winH = 400.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	vec4_t borderColor;
	if ( g_rpgBounty.isWanted ) {
		borderColor[0] = 1.00f; borderColor[1] = 0.20f; borderColor[2] = 0.20f; borderColor[3] = 0.85f;
	} else {
		borderColor[0] = 0.00f; borderColor[1] = 0.70f; borderColor[2] = 1.00f; borderColor[3] = 0.85f;
	}

	if ( s_hWantedBg <= 0 && re && re->RegisterShader ) {
		s_hWantedBg = re->RegisterShader( "gfx/rpg_hud/wanted_bg" );
	}

	if ( s_hWantedBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hWantedBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		// Header background (fallback only)
		vec4_t headerBg;
		if ( g_rpgBounty.isWanted ) {
			headerBg[0] = 0.40f; headerBg[1] = 0.08f; headerBg[2] = 0.08f; headerBg[3] = 0.88f;
		} else {
			headerBg[0] = 0.08f; headerBg[1] = 0.18f; headerBg[2] = 0.35f; headerBg[3] = 0.88f;
		}
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 22.0f, 3.0f, headerBg, NULL );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	if ( g_rpgBounty.isWanted ) {
		float titleW = SCR_GetStringWidth( "WANTED TARGETS", 8.2f );
		float titleX = winX + (winW - titleW) * 0.5f;
		SCR_DrawVirtualString( titleX, winY + 42.0f, 8.2f, "^1WANTED TARGETS", yellowCol );
	} else {
		float titleW = SCR_GetStringWidth( "ACTIVE BOUNTIES", 8.2f );
		float titleX = winX + (winW - titleW) * 0.5f;
		SCR_DrawVirtualString( titleX, winY + 42.0f, 8.2f, "^3ACTIVE BOUNTIES", yellowCol );
	}

	// Close Button instruction (nudged 3 ticks down from 18.0f)
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 21.0f, 5.0f, "^1[ESC]", yellowCol );

	// Table Headers (moved 6 ticks right, 8 ticks down from original 67.0f)
	float startY = winY + 75.0f;
	SCR_DrawVirtualString( winX + 26.0f, startY, 4.8f, "^5#", yellowCol );
	SCR_DrawVirtualString( winX + 46.0f, startY, 4.8f, "^5PLAYER NAME", yellowCol );
	if ( g_rpgBounty.isWanted ) {
		SCR_DrawVirtualString( winX + 181.0f, startY, 4.8f, "^5STREAK", yellowCol );
		SCR_DrawVirtualString( winX + winW - 84.0f, startY, 4.8f, "^5BOUNTY", yellowCol );
	} else {
		SCR_DrawVirtualString( winX + winW - 84.0f, startY, 4.8f, "^5BOUNTY REWARD", yellowCol );
	}

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY + 16.0f, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgBounty.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 70.0f, startY + 40.0f, 4.8f, "^7No active targets found.", whiteColor );
	} else {
		float rowY = startY + 20.0f;
		for ( int i = 0; i < g_rpgBounty.count && i < 10; i++ ) {
			bountyEntry_t *e = &g_rpgBounty.entries[i];

			// Highlight top 1
			if ( i == 0 ) {
				vec4_t topBg = { 0.80f, 0.60f, 0.10f, 0.15f };
				SCR_FillRect( winX + 10.0f, rowY - 1.0f, winW - 20.0f, 21.0f, topBg );
			}

			// Rank (moved 6 ticks right)
			SCR_DrawVirtualString( winX + 26.0f, rowY, 4.8f, va( "^3%d", e->rank ), whiteColor );

			// Name (moved 6 ticks right - un-truncated name printing)
			char nameFormatted[64];
			Com_sprintf( nameFormatted, sizeof( nameFormatted ), "^7%s", e->name );
			SCR_DrawVirtualString( winX + 46.0f, rowY, 4.8f, nameFormatted, whiteColor );

			// Values (moved 6 ticks right)
			if ( g_rpgBounty.isWanted ) {
				SCR_DrawVirtualString( winX + 181.0f, rowY, 4.8f, va( "^5%d wins", e->streak ), whiteColor );
				if ( e->bounty > 0 ) {
					SCR_DrawVirtualString( winX + winW - 84.0f, rowY, 4.8f, va( "^3%d CR", e->bounty ), whiteColor );
				} else {
					SCR_DrawVirtualString( winX + winW - 84.0f, rowY, 4.8f, "^7-", whiteColor );
				}
			} else {
				SCR_DrawVirtualString( winX + winW - 84.0f, rowY, 4.8f, va( "^3%d CR", e->bounty ), whiteColor );
			}

			rowY += 23.0f;
		}
	}

	// Footer instruction (nudged 5 ticks right)
	const char *closeStr = g_rpgBounty.isWanted ? "^7Press ^3F8^7, ^3ESC^7, or type ^3!wanted^7 to close" : "^7Press ^3F8^7, ^3ESC^7, or type ^3!bountylist^7 to close";
	SCR_DrawVirtualString( winX + 26.0f, winY + winH - 15.0f, 4.0f, closeStr, whiteColor );
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

	// Modal Window Dimensions (Vertical 300x400 right-aligned panel using shop_bg.tga)
	float winW = 300.0f;
	float winH = 400.0f;
	float winX = 640.0f - winW - 14.0f;
	float winY = 40.0f;

	if ( s_hShopBg <= 0 && re && re->RegisterShader ) {
		s_hShopBg = re->RegisterShader( "gfx/rpg_hud/shop_bg" );
	}

	if ( s_hShopBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hShopBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.10f, 0.75f, 0.95f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.08f, 0.20f, 0.38f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float titleW = SCR_GetStringWidth( "RANKED SHOP", 8.2f );
	float titleX = winX + (winW - titleW) * 0.5f;
	SCR_DrawVirtualString( titleX, winY + 23.0f, 8.2f, "^3RANKED SHOP", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 14.0f, 5.0f, "^1[ESC]", yellowCol );

	// Credits Balance
	char crStr[64];
	Com_sprintf( crStr, sizeof( crStr ), "^7Credits: ^5%d CR", g_rpgShop.credits );
	SCR_DrawVirtualString( winX + 12.0f, winY + 33.0f, 4.8f, crStr, whiteColor );

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 10.0f, winY + 46.0f, winW - 20.0f, 1.0f, divColor );

	// Virtual Mouse Position
	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	if ( g_rpgShop.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, winY + 120.0f, 5.2f, "^7No items.", whiteColor );
	} else {
		int maxVisible = 8;
		int startIdx = g_rpgShop.scroll;
		if ( startIdx < 0 ) startIdx = 0;

		float rowY = winY + 62.0f;
		for ( int i = startIdx; i < g_rpgShop.count && (i - startIdx) < maxVisible; i++ ) {
			rpgShopItem_t *item = &g_rpgShop.items[i];

			// Highlight active row on mouse hover
			qboolean rowHover = (qboolean)( mx >= winX + 8.0f && mx <= winX + winW - 8.0f && my >= rowY && my <= rowY + 32.0f );
			if ( rowHover ) {
				vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.15f };
				SCR_FillRect( winX + 8.0f, rowY, winW - 16.0f, 32.0f, hCol );
			}

			// Draw Item Key and Clean Display Name
			SCR_DrawVirtualString( winX + 18.0f, rowY + 2.0f, 4.4f, va( "%s", item->display ), whiteColor );

			// Draw Price info
			SCR_DrawVirtualString( winX + 18.0f, rowY + 16.0f, 4.0f, va( "^5Buy: %d cr  ^7|  ^3Sell: %d", item->price, item->sellBack ), whiteColor );

			// Draw Interactive Buttons
			float btnBuyX = winX + winW - 95.0f;
			float btnSellX = winX + winW - 55.0f;
			float btnY = rowY + 6.0f;
			float btnW = 35.0f;
			float btnH = 18.0f;

			qboolean buyHover = (qboolean)( mx >= btnBuyX && mx <= btnBuyX + btnW && my >= btnY && my <= btnY + btnH );
			qboolean sellHover = (qboolean)( mx >= btnSellX && mx <= btnSellX + btnW && my >= btnY && my <= btnY + btnH );

			vec4_t buyColor = { 0.00f, 0.50f, 0.00f, 0.40f };
			vec4_t buyColorHover = { 0.00f, 0.80f, 0.00f, 0.70f };
			vec4_t sellColor = { 0.50f, 0.00f, 0.00f, 0.40f };
			vec4_t sellColorHover = { 0.80f, 0.00f, 0.00f, 0.70f };

			// Draw BUY Button
			if ( s_hBuyBtn <= 0 && re && re->RegisterShader ) {
				s_hBuyBtn = re->RegisterShader( "gfx/rpg_hud/buy_btn" );
			}
			if ( s_hBuyBtn > 0 ) {
				vec4_t bCol = { 1.0f, 1.0f, 1.0f, buyHover ? 1.00f : 0.70f };
				re->SetColor( bCol );
				SCR_DrawPic( btnBuyX, btnY, btnW, btnH, s_hBuyBtn );
				re->SetColor( NULL );
			} else {
				SCR_DrawRoundedGlassPanel( btnBuyX, btnY, btnW, btnH, 2.0f, buyHover ? buyColorHover : buyColor, NULL );
				SCR_DrawVirtualString( btnBuyX + 6.0f, btnY + 4.0f, 3.8f, "BUY", whiteColor );
			}

			// Draw SELL Button
			if ( s_hSellBtn <= 0 && re && re->RegisterShader ) {
				s_hSellBtn = re->RegisterShader( "gfx/rpg_hud/sell_btn" );
			}
			if ( s_hSellBtn > 0 ) {
				vec4_t sCol = { 1.0f, 1.0f, 1.0f, sellHover ? 1.00f : 0.70f };
				re->SetColor( sCol );
				SCR_DrawPic( btnSellX, btnY, btnW, btnH, s_hSellBtn );
				re->SetColor( NULL );
			} else {
				SCR_DrawRoundedGlassPanel( btnSellX, btnY, btnW, btnH, 2.0f, sellHover ? sellColorHover : sellColor, NULL );
				SCR_DrawVirtualString( btnSellX + 4.0f, btnY + 4.0f, 3.8f, "SELL", whiteColor );
			}

			rowY += 34.0f;
		}

		// Draw Vertical Scroll Bar Indicator if more items than fit
		if ( g_rpgShop.count > maxVisible ) {
			float barX = winX + winW - 6.0f;
			float barY = winY + 54.0f;
			float barH = maxVisible * 34.0f - 4.0f;
			vec4_t scrollBg = { 0.05f, 0.10f, 0.20f, 0.60f };
			vec4_t scrollThumb = { 0.10f, 0.70f, 1.00f, 0.85f };
			SCR_FillRect( barX, barY, 3.0f, barH, scrollBg );

			float calcH = barH * ((float)maxVisible / (float)g_rpgShop.count);
			float thumbH = (calcH < 12.0f) ? 12.0f : calcH;

			float maxScroll = (float)(g_rpgShop.count - maxVisible);
			float thumbY = barY + (barH - thumbH) * ((float)startIdx / maxScroll);
			SCR_FillRect( barX, thumbY, 3.0f, thumbH, scrollThumb );
		}
	}

	SCR_DrawVirtualString( winX + 18.0f, winY + winH - 15.0f, 3.8f, "^7Scroll ^3[Mouse Wheel]^7 or Click buttons", whiteColor );
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

	// Modal Window Dimensions (Horizontal 380x304 centered panel using quest_bg.tga)
	float winW = 380.0f;
	float winH = 304.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 88.0f;

	if ( s_hQuestBg <= 0 && re && re->RegisterShader ) {
		s_hQuestBg = re->RegisterShader( "gfx/rpg_hud/quest_bg" );
	}

	if ( s_hQuestBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hQuestBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.20f, 0.80f, 0.50f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// Close Button instruction (nudged to Y + 14.0f to match style guidelines)
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 14.0f, 5.0f, "^1[ESC]", yellowCol );

	// Daily Quests Title Header (centered, right above the yellow underline)
	float qTitleW = SCR_GetStringWidth( "Daily Quests", 8.5f );
	float qTitleX = winX + (winW - qTitleW) * 0.5f;
	SCR_DrawVirtualString( qTitleX, winY + 32.0f, 8.5f, "^3Daily Quests", yellowCol );

	// Daily Quests Content
	if ( g_rpgQuest.questCount == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, winY + 80.0f, 5.2f, "^7No daily quests available.", whiteColor );
	} else {
		for ( int i = 0; i < g_rpgQuest.questCount && i < 3; i++ ) {
			rpgQuestEntry_t *q = &g_rpgQuest.quests[i];
			float boxY = winY + 58.0f + (i * 78.0f);

			if ( q->done ) {
				// Line 1: Description (size 6.2f, nudged right)
				SCR_DrawVirtualString( winX + 34.0f, boxY + 14.0f, 6.2f, va( "^3%d. ^2[DONE] ^7%s", q->id, q->desc ), whiteColor );
				// Line 2: Progress & Rewards (size 5.4f, nudged right)
				SCR_DrawVirtualString( winX + 40.0f, boxY + 42.0f, 5.4f, "^2Completed!", whiteColor );
				SCR_DrawVirtualString( winX + 175.0f, boxY + 42.0f, 5.4f, va( "^5Rewards: ^3+%d CR ^2+%d Elo", q->rewardCr, q->rewardFr ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 90.0f, boxY + 42.0f, 5.4f, va( "^6[%s]", q->mode ), whiteColor );
			} else {
				// Line 1: Description (size 6.2f, nudged right)
				SCR_DrawVirtualString( winX + 34.0f, boxY + 14.0f, 6.2f, va( "^3%d. ^7%s", q->id, q->desc ), whiteColor );
				// Line 2: Progress & Rewards (size 5.4f, nudged right)
				SCR_DrawVirtualString( winX + 40.0f, boxY + 42.0f, 5.4f, va( "^5Progress: %d/%d", q->prog, q->goal ), whiteColor );
				SCR_DrawVirtualString( winX + 175.0f, boxY + 42.0f, 5.4f, va( "^5Rewards: ^3+%d CR ^2+%d Elo", q->rewardCr, q->rewardFr ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 90.0f, boxY + 42.0f, 5.4f, va( "^6[%s]", q->mode ), whiteColor );
			}
		}
	}

	SCR_DrawVirtualString( winX + 21.0f, winY + winH - 15.0f, 3.8f, "^7Complete daily quests to earn rewards  |  ^3ESC^7 to close", whiteColor );
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

	// Modal Window Dimensions (Vertical 240x350 right-aligned panel using inventory_bg.tga)
	float winW = 240.0f;
	float winH = 350.0f;
	float winX = 640.0f - winW - 14.0f; // Right-aligned side panel!
	float winY = 50.0f;

	if ( s_hInventoryBg <= 0 && re && re->RegisterShader ) {
		s_hInventoryBg = re->RegisterShader( "gfx/rpg_hud/inventory_bg" );
	}

	if ( s_hInventoryBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hInventoryBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.10f, 0.75f, 0.95f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.08f, 0.20f, 0.38f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float titleW = SCR_GetStringWidth( "INVENTORY", 8.5f );
	float titleX = winX + (winW - titleW) * 0.5f;
	SCR_DrawVirtualString( titleX, winY + 23.0f, 8.5f, "^3INVENTORY", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 14.0f, 5.0f, "^1[ESC]", yellowCol );

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 10.0f, winY + 34.0f, winW - 20.0f, 1.0f, divColor );

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	if ( g_rpgInventory.invCount == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, winY + 120.0f, 5.2f, "^7Inventory empty.", whiteColor );
	} else {
		int maxVisible = 6;
		int startIdx = g_rpgInventory.scroll;
		if ( startIdx < 0 ) startIdx = 0;

		float rowY = winY + 68.0f;
		for ( int i = startIdx; i < g_rpgInventory.invCount && (i - startIdx) < maxVisible; i++ ) {
			rpgInvEntry_t *item = &g_rpgInventory.inv[i];

			char cleanDisplay[256];
			Q_strncpyz( cleanDisplay, item->display, sizeof(cleanDisplay) );
			if ( strstr( cleanDisplay, "5DB7A2" ) || strstr( cleanDisplay, "XP Boost" ) ) {
				Com_sprintf( cleanDisplay, sizeof(cleanDisplay), "^2XP Boost ^7(50+ for 1 round)" );
			}

			qboolean rowHover = (qboolean)( mx >= winX + 8.0f && mx <= winX + winW - 8.0f && my >= rowY && my <= rowY + 34.0f );
			if ( rowHover ) {
				vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.15f };
				SCR_FillRect( winX + 8.0f, rowY, winW - 16.0f, 34.0f, hCol );
			}

			// Item Name and Qty (shifted right to avoid overlaying left border)
			SCR_DrawVirtualString( winX + 25.0f, rowY + 2.0f, 4.8f, cleanDisplay, whiteColor );
			SCR_DrawVirtualString( winX + 25.0f, rowY + 16.0f, 4.2f, va( "^5Owned: x%d", item->qty ), whiteColor );

			// Interactive USE Button (shifted left to avoid overlaying right border)
			float btnUseX = winX + winW - 71.0f;
			float btnY = rowY + 8.0f;
			float btnW = 45.0f;
			float btnH = 18.0f;

			qboolean useHover = (qboolean)( mx >= btnUseX && mx <= btnUseX + btnW && my >= btnY && my <= btnY + btnH );

			if ( s_hUseBtn <= 0 && re && re->RegisterShader ) {
				s_hUseBtn = re->RegisterShader( "gfx/rpg_hud/use_btn" );
			}
			if ( s_hUseBtn > 0 ) {
				vec4_t uCol = { 1.0f, 1.0f, 1.0f, useHover ? 1.00f : 0.70f };
				re->SetColor( uCol );
				SCR_DrawPic( btnUseX, btnY, btnW, btnH, s_hUseBtn );
				re->SetColor( NULL );
			} else {
				vec4_t useCol = { 0.00f, 0.50f, 0.00f, 0.40f };
				vec4_t useColHover = { 0.00f, 0.80f, 0.00f, 0.70f };
				SCR_DrawRoundedGlassPanel( btnUseX, btnY, btnW, btnH, 2.0f, useHover ? useColHover : useCol, NULL );
				SCR_DrawVirtualString( btnUseX + 11.0f, btnY + 4.0f, 3.8f, "USE", whiteColor );
			}

			rowY += 36.0f;
		}

		// Scroll Bar
		if ( g_rpgInventory.invCount > maxVisible ) {
			float barX = winX + winW - 6.0f;
			float barY = winY + 42.0f;
			float barH = maxVisible * 34.0f - 4.0f;
			vec4_t scrollBg = { 0.05f, 0.10f, 0.20f, 0.60f };
			vec4_t scrollThumb = { 0.10f, 0.70f, 1.00f, 0.85f };
			SCR_FillRect( barX, barY, 3.0f, barH, scrollBg );

			float calcH = barH * ((float)maxVisible / (float)g_rpgInventory.invCount);
			float thumbH = (calcH < 12.0f) ? 12.0f : calcH;

			float maxScroll = (float)(g_rpgInventory.invCount - maxVisible);
			float thumbY = barY + (barH - thumbH) * ((float)startIdx / maxScroll);
			SCR_FillRect( barX, thumbY, 3.0f, thumbH, scrollThumb );
		}
	}

	SCR_DrawVirtualString( winX + 22.0f, winY + winH - 14.0f, 3.8f, "^7Scroll ^3[Mouse Wheel]^7 or Click buttons", whiteColor );
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

	float winW = 270.0f;
	float winH = 360.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 60.0f;

	if ( s_hAchBg <= 0 && re && re->RegisterShader ) {
		s_hAchBg = re->RegisterShader( "gfx/rpg_hud/ach_bg" );
	}

	if ( s_hAchBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hAchBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.85f, 0.65f, 0.10f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.35f, 0.25f, 0.05f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// Top corner red ESC close button (drawn at winY + 9.0f)
	SCR_DrawVirtualString( winX + winW - 35.0f, winY + 9.0f, 4.4f, "^1[ESC]", yellowCol );

	// Title header centered in gold header box (at winY + 22.0f)
	float achTitleW = SCR_GetStringWidth( "ACHIEVEMENTS", 6.5f );
	float achTitleX = winX + (winW - achTitleW) * 0.5f;
	SCR_DrawVirtualString( achTitleX, winY + 22.0f, 6.5f, "^3ACHIEVEMENTS", yellowCol );

	if ( g_rpgAch.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, winY + 100.0f, 5.2f, "^7No achievements loaded.", whiteColor );
	} else {
		// Shifted down to winY + 68.0f below header frame line
		float rowY = winY + 68.0f;
		for ( int i = 0; i < g_rpgAch.count && i < 9; i++ ) {
			rpgAchEntry_t *e = &g_rpgAch.entries[i];

			// Shifted to winX + 26.0f to prevent left border collision
			if ( e->unlocked ) {
				SCR_DrawVirtualString( winX + 26.0f, rowY, 4.0f, va( "^2[UNLOCKED] ^7%s", e->name ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 68.0f, rowY, 4.0f, va( "^5+%d CR", e->rewardCr ), whiteColor );
			} else {
				SCR_DrawVirtualString( winX + 26.0f, rowY, 4.0f, va( "^7[LOCKED] %s", e->name ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 68.0f, rowY, 4.0f, va( "^3+%d CR", e->rewardCr ), whiteColor );
			}
			rowY += 18.0f;
		}
	}

	// Press ESC text centered at bottom
	float escW = SCR_GetStringWidth( "Press [ESC] to close", 4.5f );
	SCR_DrawVirtualString( winX + (winW - escW) * 0.5f, winY + winH - 13.0f, 4.5f, "^7Press ^1[ESC]^7 to close", whiteColor );
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

	// Modal Window Dimensions (Vertical 290x400 centered card using leaderboard_bg.tga)
	float winW = 290.0f;
	float winH = 400.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	static qhandle_t s_hLeaderboardBg = 0;
	if ( s_hLeaderboardBg <= 0 && re && re->RegisterShader ) {
		s_hLeaderboardBg = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );
	}

	if ( s_hLeaderboardBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hLeaderboardBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.20f, 0.85f, 0.40f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float titleW = SCR_GetStringWidth( "TOP WEALTH", 8.2f );
	float titleX = winX + (winW - titleW) * 0.5f;
	SCR_DrawVirtualString( titleX, winY + 23.0f, 8.2f, "^2TOP WEALTH", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 14.0f, 5.0f, "^1[ESC]", yellowCol );

	vec4_t divColor = { 0.20f, 0.85f, 0.40f, 0.70f };
	float colY = winY + 62.0f;

	// Headers: # | PLAYER NAME | CREDITS
	SCR_DrawVirtualString( winX + 25.0f, colY, 4.8f, "^5#", whiteColor );
	SCR_DrawVirtualString( winX + 60.0f, colY, 4.8f, "^5PLAYER NAME", whiteColor );
	SCR_DrawVirtualString( winX + winW - 80.0f, colY, 4.8f, "^5CREDITS", whiteColor );

	float rowStartY = colY + 20.0f;
	float rowHeight = 23.0f;

	for ( int i = 0; i < 10; i++ ) {
		float currentY = rowStartY + (i * rowHeight);

		if ( i % 2 == 0 ) {
			vec4_t rowBg = { 0.05f, 0.20f, 0.10f, 0.25f };
			SCR_FillRect( winX + 10.0f, currentY, winW - 20.0f, rowHeight - 2.0f, rowBg );
		}

		if ( i < g_rpgTopCredits.count ) {
			topCreditsEntry_t *e = &g_rpgTopCredits.entries[i];
			// Rank
			char numStr[16];
			Com_sprintf( numStr, sizeof(numStr), (i < 3) ? "^3#%d" : "^7#%d", i + 1 );
			SCR_DrawVirtualString( winX + 25.0f, currentY + 2.0f, 4.8f, numStr, whiteColor );
			
			// Name
			char pNameStr[64];
			Com_sprintf( pNameStr, sizeof(pNameStr), "^7%.20s", e->name );
			SCR_DrawVirtualString( winX + 60.0f, currentY + 2.0f, 4.8f, pNameStr, whiteColor );
			
			// Credits
			SCR_DrawVirtualString( winX + winW - 80.0f, currentY + 2.0f, 4.8f, va( "^5%d CR", e->credits ), whiteColor );
		}
	}

	SCR_DrawVirtualString( winX + 21.0f, winY + winH - 15.0f, 4.0f, "^7Press ^1[ESC]^7 to close Wealth Leaderboard", whiteColor );
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

	// Modal Window Dimensions (Vertical 290x400 centered card using leaderboard_bg.tga)
	float winW = 290.0f;
	float winH = 400.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 40.0f;

	static qhandle_t s_hLeaderboardBg = 0;
	if ( s_hLeaderboardBg <= 0 && re && re->RegisterShader ) {
		s_hLeaderboardBg = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );
	}

	if ( s_hLeaderboardBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hLeaderboardBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 1.00f, 0.45f, 0.10f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	float titleW = SCR_GetStringWidth( "TOP POTATO", 8.2f );
	float titleX = winX + (winW - titleW) * 0.5f;
	SCR_DrawVirtualString( titleX, winY + 23.0f, 8.2f, "^1TOP POTATO", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 14.0f, 5.0f, "^1[ESC]", yellowCol );

	vec4_t divColor = { 1.00f, 0.45f, 0.10f, 0.70f };
	float colY = winY + 62.0f;
	SCR_FillRect( winX + 10.0f, colY + 16.0f, winW - 20.0f, 1.0f, divColor );

	// Headers: # | PLAYER NAME | TICKS
	SCR_DrawVirtualString( winX + 15.0f, colY, 4.8f, "^5#", whiteColor );
	SCR_DrawVirtualString( winX + 35.0f, colY, 4.8f, "^5PLAYER NAME", whiteColor );
	SCR_DrawVirtualString( winX + winW - 95.0f, colY, 4.8f, "^5TICKS", whiteColor );

	float rowStartY = colY + 20.0f;
	float rowHeight = 23.0f;

	for ( int i = 0; i < 10; i++ ) {
		float currentY = rowStartY + (i * rowHeight);

		if ( i % 2 == 0 ) {
			vec4_t rowBg = { 0.25f, 0.10f, 0.05f, 0.25f };
			SCR_FillRect( winX + 10.0f, currentY, winW - 20.0f, rowHeight - 2.0f, rowBg );
		}

		if ( i < g_rpgTopPotato.count ) {
			topPotatoEntry_t *e = &g_rpgTopPotato.entries[i];
			// Rank
			char numStr[16];
			Com_sprintf( numStr, sizeof(numStr), (i < 3) ? "^3#%d" : "^7#%d", i + 1 );
			SCR_DrawVirtualString( winX + 15.0f, currentY + 2.0f, 4.8f, numStr, whiteColor );
			
			// Name
			char pNameStr[64];
			Com_sprintf( pNameStr, sizeof(pNameStr), "^7%.20s", e->name );
			SCR_DrawVirtualString( winX + 35.0f, currentY + 2.0f, 4.8f, pNameStr, whiteColor );
			
			// Ticks
			SCR_DrawVirtualString( winX + winW - 95.0f, currentY + 2.0f, 4.8f, va( "%d", e->ticks ), whiteColor );
		}
	}

	SCR_DrawVirtualString( winX + 21.0f, winY + winH - 15.0f, 4.0f, "^7Press ^1[ESC]^7 to close Potato Leaderboard", whiteColor );
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

	float winW = 412.0f;
	float winH = 220.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 130.0f;

	if ( s_hAdvBg <= 0 && re && re->RegisterShader ) {
		s_hAdvBg = re->RegisterShader( "gfx/rpg_hud/adv_bg" );
	}

	if ( s_hAdvBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hAdvBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.92f };
		vec4_t borderColor = { 0.70f, 0.30f, 0.90f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.30f, 0.10f, 0.40f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );
	}

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 15.0f, 5.2f, va( "^6ADVENTURE: ^3%s", g_rpgAdv.title[0] ? g_rpgAdv.title : "Quest" ), yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 15.0f, 5.0f, "^1[ESC]", yellowCol );

	// Story Text (word-wrapped to prevent extending outside UI borders)
	float startY = winY + 36.0f;
	g_rpgAdv.choiceY = SCR_DrawVirtualStringWrapped( winX + 16.0f, startY, 4.4f, winW - 32.0f, g_rpgAdv.text, whiteColor ) + 8.0f;
	float choiceY = g_rpgAdv.choiceY;

	// Virtual Mouse Position
	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	// Choices (positioned dynamically relative to end of wrapped story text)
	if ( g_rpgAdv.choice1[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 6.0f && my <= choiceY + 12.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 6.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^5[1] %s", g_rpgAdv.choice1 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^3[1] %s", g_rpgAdv.choice1 ), whiteColor );
		}
		choiceY += 18.0f;
	}
	if ( g_rpgAdv.choice2[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 6.0f && my <= choiceY + 12.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 6.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^5[2] %s", g_rpgAdv.choice2 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^3[2] %s", g_rpgAdv.choice2 ), whiteColor );
		}
		choiceY += 18.0f;
	}
	if ( g_rpgAdv.choice3[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 6.0f && my <= choiceY + 12.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 6.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^5[3] %s", g_rpgAdv.choice3 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 5.2f, va( "^3[3] %s", g_rpgAdv.choice3 ), whiteColor );
		}
		choiceY += 18.0f;
	}

	SCR_DrawVirtualString( winX + 20.0f, winY + winH - 12.0f, 4.5f, "^7Click or press ^31^7, ^32^7, ^33^7 to choose  |  ^1!adv^7 or ^1[ESC]^7 to close", whiteColor );
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
	if ( hover && s_hSettingsBtnHover > 0 ) {
		SCR_DrawPic( x, y, w, h, s_hSettingsBtnHover );
	} else if ( s_hSettingsBtnNormal > 0 ) {
		SCR_DrawPic( x, y, w, h, s_hSettingsBtnNormal );
	} else {
		vec4_t bg = { 0.1f, 0.2f, 0.3f, 0.8f };
		vec4_t border = { 0.0f, 0.8f, 1.0f, 1.0f };
		if ( hover ) {
			bg[0] += 0.1f; bg[1] += 0.1f; bg[2] += 0.1f;
		}
		SCR_DrawRoundedGlassPanel( x, y, w, h, 4.0f, bg, border );
	}
	float strW = SCR_GetStringWidth( text, textSz );
	vec4_t white = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( x + (w - strW) * 0.5f, y + (h - textSz * 1.1f) * 0.5f, textSz, text, white );
}

static void SCR_DrawSettingSlider( float x, float y, float w, float h, float progress, const char *label, const char *valueText ) {
	vec4_t labelCol = { 0.8f, 0.8f, 0.8f, 1.0f };
	SCR_DrawVirtualString( x, y - 10.0f, 4.0f, label, labelCol );
	SCR_DrawVirtualString( x + w - SCR_GetStringWidth( valueText, 4.0f ), y - 10.0f, 4.0f, valueText, labelCol );

	if ( s_hSettingsSliderTrack > 0 ) {
		SCR_DrawPic( x, y, w, h, s_hSettingsSliderTrack );
	} else {
		vec4_t trackCol = { 0.05f, 0.1f, 0.15f, 1.0f };
		SCR_FillRect( x, y + h * 0.4f, w, h * 0.2f, trackCol );
	}

	float thumbX = x + progress * (w - 20.0f);
	if ( s_hSettingsSliderThumb > 0 ) {
		SCR_DrawPic( thumbX, y - (20.0f - h) * 0.5f, 20.0f, 20.0f, s_hSettingsSliderThumb );
	} else {
		vec4_t thumbCol = { 1.0f, 1.0f, 1.0f, 1.0f };
		SCR_FillRect( thumbX, y - 4.0f, 20.0f, h + 8.0f, thumbCol );
	}
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
	const char *styleLabel = "STYLE: CLASSIC";
	if ( curStyle == 1 ) styleLabel = "STYLE: SLEEK";
	else if ( curStyle == 2 ) styleLabel = "STYLE: TECH";
	else if ( curStyle == 3 ) styleLabel = "STYLE: MINIMAL";

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
SCR_DrawRPGMenuOverlay

Master RPG Hub Menu (!rpgmenu / !menu / !settings)
==================
*/
void SCR_DrawRPGMenuOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !g_rpgMenu.active ) return;

	float winW = 540.0f;
	float winH = 390.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 240.0f - winH * 0.5f;

	vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.94f };
	vec4_t borderColor = { 0.10f, 0.75f, 0.95f, 0.90f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t cyanColor = { 0.10f, 0.80f, 1.00f, 1.0f };

	SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 8.0f, bgColor, borderColor );

	// Title
	float titleW = SCR_GetStringWidth( "MASTER RPG HUB", 8.0f );
	SCR_DrawVirtualString( winX + (winW - titleW) * 0.5f, winY + 16.0f, 8.0f, "^3MASTER RPG HUB", yellowCol );
	SCR_DrawVirtualString( winX + winW - 35.0f, winY + 10.0f, 5.0f, "^1[ESC]", yellowCol );

	// Navigation Tabs
	const char *tabs[4] = { "1. PROFILE", "2. SETTINGS", "3. RANKS", "4. COMMANDS" };
	float tabW = 120.0f;
	float tabY = winY + 38.0f;
	float tabStartX = winX + (winW - (4 * tabW + 3 * 8.0f)) * 0.5f;

	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;

	for ( int i = 0; i < 4; i++ ) {
		float tx = tabStartX + i * (tabW + 8.0f);
		qboolean isSelected = (qboolean)(g_rpgMenu.tab == i);
		qboolean isHover = (qboolean)(mx >= tx && mx <= tx + tabW && my >= tabY && my <= tabY + 22.0f);

		vec4_t tabBg = { 0.05f, 0.15f, 0.28f, isSelected ? 0.90f : (isHover ? 0.60f : 0.30f) };
		vec4_t tabBorder = { 0.10f, 0.75f, 0.95f, isSelected ? 0.95f : 0.40f };
		SCR_DrawRoundedGlassPanel( tx, tabY, tabW, 22.0f, 4.0f, tabBg, tabBorder );

		float strW = SCR_GetStringWidth( tabs[i], 4.2f );
		SCR_DrawVirtualString( tx + (tabW - strW) * 0.5f, tabY + 5.0f, 4.2f, isSelected ? va("^3%s", tabs[i]) : va("^7%s", tabs[i]), whiteColor );
	}

	SCR_FillRect( winX + 15.0f, tabY + 26.0f, winW - 30.0f, 1.0f, borderColor );

	float contentY = tabY + 34.0f;

	// Tab Content
	if ( g_rpgMenu.tab == 0 ) {
		// PROFILE & CREDENTIALS TAB (!stats & !details sync)
		float boxX = winX + 25.0f;
		float boxW = winW - 50.0f;
		
		SCR_DrawVirtualString( boxX, contentY, 5.5f, "^5[PLAYER ACCOUNT & STATS PROFILE]", cyanColor );
		
		int level = g_rpgStats.level > 0 ? g_rpgStats.level : (cg_rpg_level ? cg_rpg_level->integer : 1);
		int xp = g_rpgStats.xp > 0 ? g_rpgStats.xp : (cg_rpg_xp ? cg_rpg_xp->integer : 0);
		int xpMax = cg_rpg_xp_max ? cg_rpg_xp_max->integer : 1000;
		int fr = g_rpgStats.fr > 0 ? g_rpgStats.fr : (cg_rpg_fr ? cg_rpg_fr->integer : 1000);
		const char *name = g_rpgStats.name[0] ? g_rpgStats.name : ((cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : "Player");
		const char *rank = g_rpgStats.rankTitle[0] ? g_rpgStats.rankTitle : ((cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : "Padawan");
		int credits = g_rpgStats.credits > 0 ? g_rpgStats.credits : g_rpgShop.credits;

		SCR_DrawVirtualString( boxX + 10.0f, contentY + 20.0f, 4.4f, va("^7Character Name: ^3%s", name), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 36.0f, 4.4f, va("^7Rank Title:     ^3%s", rank), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 52.0f, 4.4f, va("^7Current Level:  ^2Level %d", level), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 68.0f, 4.8f, va("^7Elo Rating:     ^5%d Elo", fr), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 84.0f, 4.8f, va("^7Credits:        ^5%d CR", credits), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 100.0f, 4.8f, va("^7Duel Record:    ^2%d W ^7/ ^1%d L ^7(^3%d Kills^7 / ^1%d Deaths^7)", g_rpgStats.wins, g_rpgStats.losses, g_rpgStats.kills, g_rpgStats.deaths), whiteColor );

		// XP Progress Bar
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 120.0f, 4.2f, "^7XP Threshold Progress:", whiteColor );
		float barX = boxX + 10.0f;
		float barY = contentY + 134.0f;
		float barW = boxW - 20.0f;
		float barH = 12.0f;

		vec4_t barBg = { 0.05f, 0.10f, 0.20f, 0.80f };
		vec4_t barBorder = { 0.20f, 0.70f, 1.00f, 0.60f };
		SCR_DrawRoundedGlassPanel( barX, barY, barW, barH, 2.0f, barBg, barBorder );

		int relativeXP = cg_rpg_xp ? cg_rpg_xp->integer : 0;
		int relativeXPMax = cg_rpg_xp_max ? cg_rpg_xp_max->integer : 1000;
		if ( relativeXP < 0 ) relativeXP = 0;
		if ( relativeXP > relativeXPMax ) relativeXP = relativeXPMax;

		float ratio = (relativeXPMax > 0) ? ((float)relativeXP / (float)relativeXPMax) : 0.0f;
		if ( ratio > 1.0f ) ratio = 1.0f;
		if ( ratio > 0.0f ) {
			vec4_t fillCol = { 0.10f, 0.80f, 0.30f, 0.90f };
			SCR_FillRect( barX + 1.0f, barY + 1.0f, (barW - 2.0f) * ratio, barH - 2.0f, fillCol );
		}

		char xpStr[64];
		Com_sprintf( xpStr, sizeof(xpStr), "^7%d / %d XP (%d%%)", relativeXP, relativeXPMax, (int)(ratio * 100.0f) );
		float xpW = SCR_GetStringWidth( xpStr, 4.0f );
		SCR_DrawVirtualString( barX + (barW - xpW) * 0.5f, barY + 2.0f, 4.0f, xpStr, whiteColor );

		// Login Credentials Box (retrieved from database)
		vec4_t credBg = { 0.08f, 0.18f, 0.10f, 0.35f };
		vec4_t credBorder = { 0.20f, 0.80f, 0.40f, 0.50f };
		SCR_DrawRoundedGlassPanel( boxX, contentY + 158.0f, boxW, 58.0f, 4.0f, credBg, credBorder );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 164.0f, 4.4f, "^2[ACCOUNT CREDENTIALS]", whiteColor );

		const char *uName = g_rpgStats.loginUser[0] ? g_rpgStats.loginUser : "Guest";
		const char *uPass = g_rpgStats.loginPass[0] ? g_rpgStats.loginPass : "None";

		SCR_DrawVirtualString( boxX + 10.0f, contentY + 180.0f, 4.2f, va("^7Username: ^3%s", uName), whiteColor );
		SCR_DrawVirtualString( boxX + 10.0f, contentY + 194.0f, 4.2f, va("^7Password: ^3%s", uPass), whiteColor );
		// Button: Toggle Ranked Mode
		float btnX = boxX;
		float btnY = contentY + 224.0f;
		float btnW = boxW;
		float btnH = 24.0f;

		extern int g_clientRankedEnabled;
		qboolean btnHover = (qboolean)(mx >= btnX && mx <= btnX + btnW && my >= btnY && my <= btnY + btnH);
		vec4_t btnBgRanked = { 0.08f, 0.40f, 0.20f, btnHover ? 0.95f : 0.80f };
		vec4_t btnBgCasual = { 0.40f, 0.20f, 0.08f, btnHover ? 0.95f : 0.80f };
		vec4_t btnBorderRanked = { 0.20f, 1.00f, 0.40f, btnHover ? 1.00f : 0.70f };
		vec4_t btnBorderCasual = { 1.00f, 0.60f, 0.20f, btnHover ? 1.00f : 0.70f };

		if ( g_clientRankedEnabled ) {
			SCR_DrawRoundedGlassPanel( btnX, btnY, btnW, btnH, 4.0f, btnBgRanked, btnBorderRanked );
			const char *btnLabel = "^2[ RANKED MATCHES: ENABLED ]";
			float labelW = SCR_GetStringWidth( btnLabel, 4.4f );
			SCR_DrawVirtualString( btnX + (btnW - labelW) * 0.5f, btnY + 4.0f, 4.4f, btnLabel, whiteColor );
		} else {
			SCR_DrawRoundedGlassPanel( btnX, btnY, btnW, btnH, 4.0f, btnBgCasual, btnBorderCasual );
			const char *btnLabel = "^3[ CASUAL MODE (NO ELO) ]";
			float labelW = SCR_GetStringWidth( btnLabel, 4.4f );
			SCR_DrawVirtualString( btnX + (btnW - labelW) * 0.5f, btnY + 4.0f, 4.4f, btnLabel, whiteColor );
		}

	} else if ( g_rpgMenu.tab == 1 ) {
		// SETTINGS TAB (Unified HUD Tuner & Sliders)
		SCR_DrawVirtualString( winX + 25.0f, contentY, 5.2f, "^5[HUD CONFIGURATION STUDIO]", cyanColor );
		
		int curStyle = cg_rpg_style ? cg_rpg_style->integer : 0;
		const char *styleNames[4] = { "Classic", "Sleek Bar", "Tech Hex", "Minimal" };
		const char *curPos = (cg_rpg_pos && cg_rpg_pos->string[0]) ? cg_rpg_pos->string : "left";

		// Button 1: HUD Style
		qboolean h1 = (qboolean)(mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 20.0f && my <= contentY + 55.0f);
		SCR_DrawRoundedGlassPanel( winX + 30.0f, contentY + 20.0f, 210.0f, 35.0f, 4.0f, h1 ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( winX + 40.0f, contentY + 32.0f, 4.2f, va("^7HUD Style: ^3%s", styleNames[curStyle % 4]), whiteColor );

		// Button 2: HUD Position
		qboolean h2 = (qboolean)(mx >= winX + 270.0f && mx <= winX + 480.0f && my >= contentY + 20.0f && my <= contentY + 55.0f);
		SCR_DrawRoundedGlassPanel( winX + 270.0f, contentY + 20.0f, 210.0f, 35.0f, 4.0f, h2 ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( winX + 280.0f, contentY + 32.0f, 4.2f, va("^7Position: ^3%s", curPos), whiteColor );

		// Load slider textures
		if ( s_hSettingsSliderTrack <= 0 && re && re->RegisterShader ) {
			s_hSettingsSliderTrack = re->RegisterShader( "gfx/rpg_hud/hud_slider_track" );
			s_hSettingsSliderThumb = re->RegisterShader( "gfx/rpg_hud/hud_slider_thumb" );
		}

		static cvar_t *cg_partyX = NULL;
		static cvar_t *cg_partyY = NULL;
		if ( !cg_partyX ) cg_partyX = Cvar_Get( "cg_partyX", "15", CVAR_ARCHIVE );
		if ( !cg_partyY ) cg_partyY = Cvar_Get( "cg_partyY", "140", CVAR_ARCHIVE );

		// Dragging slider logic
		float col2X = winX + 270.0f;
		if ( kg.keys[A_MOUSE1].down ) {
			if ( !s_draggingSliderX && !s_draggingSliderY && !s_draggingPartySliderX && !s_draggingPartySliderY ) {
				if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 60.0f && my <= contentY + 80.0f ) {
					s_draggingSliderX = qtrue;
				}
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 95.0f && my <= contentY + 115.0f ) {
					s_draggingSliderY = qtrue;
				}
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 130.0f && my <= contentY + 150.0f ) {
					s_draggingPartySliderX = qtrue;
				}
				else if ( mx >= col2X && mx <= col2X + 200.0f && my >= contentY + 165.0f && my <= contentY + 185.0f ) {
					s_draggingPartySliderY = qtrue;
				}
			}

			if ( s_draggingSliderX ) {
				float newProgress = (mx - col2X) / 200.0f;
				if ( newProgress < 0.0f ) newProgress = 0.0f;
				if ( newProgress > 1.0f ) newProgress = 1.0f;
				Cvar_SetValue( "cg_rpg_x", newProgress * 640.0f );
			}
			if ( s_draggingSliderY ) {
				float newProgress = (mx - col2X) / 200.0f;
				if ( newProgress < 0.0f ) newProgress = 0.0f;
				if ( newProgress > 1.0f ) newProgress = 1.0f;
				Cvar_SetValue( "cg_rpg_y", newProgress * 480.0f );
			}
			if ( s_draggingPartySliderX ) {
				float newProgress = (mx - col2X) / 200.0f;
				if ( newProgress < 0.0f ) newProgress = 0.0f;
				if ( newProgress > 1.0f ) newProgress = 1.0f;
				Cvar_SetValue( "cg_partyX", newProgress * 640.0f );
			}
			if ( s_draggingPartySliderY ) {
				float newProgress = (mx - col2X) / 200.0f;
				if ( newProgress < 0.0f ) newProgress = 0.0f;
				if ( newProgress > 1.0f ) newProgress = 1.0f;
				Cvar_SetValue( "cg_partyY", newProgress * 480.0f );
			}
		} else {
			s_draggingSliderX = qfalse;
			s_draggingSliderY = qfalse;
			s_draggingPartySliderX = qfalse;
			s_draggingPartySliderY = qfalse;
		}

		// Slider 1: HUD X Offset Tuner
		float curX = cg_rpg_x ? cg_rpg_x->value : 14.0f;
		if ( curX < 0.0f ) curX = 0.0f;
		if ( curX > 640.0f ) curX = 640.0f;
		char xText[32];
		Com_sprintf( xText, sizeof( xText ), "%d px", (int)curX );
		SCR_DrawSettingSlider( col2X, contentY + 65.0f, 200.0f, 12.0f, curX / 640.0f, "HUD X COORDINATE", xText );

		// Slider 2: HUD Y Offset Tuner
		float curY = cg_rpg_y ? cg_rpg_y->value : 14.0f;
		if ( curY < 0.0f ) curY = 0.0f;
		if ( curY > 480.0f ) curY = 480.0f;
		char yText[32];
		Com_sprintf( yText, sizeof( yText ), "%d px", (int)curY );
		SCR_DrawSettingSlider( col2X, contentY + 100.0f, 200.0f, 12.0f, curY / 480.0f, "HUD Y COORDINATE", yText );

		// Slider 3: Party UI X Position
		float curPX = cg_partyX ? cg_partyX->value : 15.0f;
		if ( curPX < 0.0f ) curPX = 0.0f;
		if ( curPX > 640.0f ) curPX = 640.0f;
		char pxText[32];
		Com_sprintf( pxText, sizeof( pxText ), "%d px", (int)curPX );
		SCR_DrawSettingSlider( col2X, contentY + 135.0f, 200.0f, 12.0f, curPX / 640.0f, "PARTY UI X POSITION", pxText );

		// Slider 4: Party UI Y Position
		float curPY = cg_partyY ? cg_partyY->value : 140.0f;
		if ( curPY < 0.0f ) curPY = 0.0f;
		if ( curPY > 480.0f ) curPY = 480.0f;
		char pyText[32];
		Com_sprintf( pyText, sizeof( pyText ), "%d px", (int)curPY );
		SCR_DrawSettingSlider( col2X, contentY + 170.0f, 200.0f, 12.0f, curPY / 480.0f, "PARTY UI Y POSITION", pyText );

		// Avatar Preview Box
		SCR_DrawVirtualString( winX + 30.0f, contentY + 115.0f, 4.2f, "^7Avatar Selection:", whiteColor );
		float avBoxX = winX + 160.0f;
		float avBoxY = contentY + 108.0f;
		if ( s_hAvatar > 0 ) {
			SCR_DrawPic( avBoxX, avBoxY, 34.0f, 34.0f, s_hAvatar );
		}

		// Avatar Prev / Next Buttons
		qboolean hAvL = (qboolean)(mx >= avBoxX - 45.0f && mx <= avBoxX - 10.0f && my >= avBoxY + 3.0f && my <= avBoxY + 33.0f);
		SCR_DrawRoundedGlassPanel( avBoxX - 45.0f, avBoxY + 3.0f, 35.0f, 30.0f, 3.0f, hAvL ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( avBoxX - 32.0f, avBoxY + 10.0f, 5.0f, "<", whiteColor );

		qboolean hAvR = (qboolean)(mx >= avBoxX + 45.0f && mx <= avBoxX + 80.0f && my >= avBoxY + 3.0f && my <= avBoxY + 33.0f);
		SCR_DrawRoundedGlassPanel( avBoxX + 45.0f, avBoxY + 3.0f, 35.0f, 30.0f, 3.0f, hAvR ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( avBoxX + 58.0f, avBoxY + 10.0f, 5.0f, ">", whiteColor );

		// Chat Box Customizer Buttons
		cvar_t *cPosCv = Cvar_Get( "cg_chat_pos", "0", CVAR_ARCHIVE );
		cvar_t *cStyCv = Cvar_Get( "cg_chat_style", "0", CVAR_ARCHIVE );
		const char *cPosNames[3] = { "Bottom", "Top", "Classic" };
		const char *cStyNames[4] = { "Dark Glass", "Cyber Cyan", "Solid Black", "No BG" };

		// Chat Position Button
		qboolean hCPos = (qboolean)(mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 160.0f && my <= contentY + 193.0f);
		SCR_DrawRoundedGlassPanel( winX + 30.0f, contentY + 160.0f, 210.0f, 33.0f, 4.0f, hCPos ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( winX + 40.0f, contentY + 171.0f, 4.0f, va("^7Chat Pos: ^3%s", cPosNames[cPosCv->integer % 3]), whiteColor );

		// Chat Style Button
		qboolean hCSty = (qboolean)(mx >= winX + 30.0f && mx <= winX + 240.0f && my >= contentY + 200.0f && my <= contentY + 233.0f);
		SCR_DrawRoundedGlassPanel( winX + 30.0f, contentY + 200.0f, 210.0f, 33.0f, 4.0f, hCSty ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( winX + 40.0f, contentY + 211.0f, 4.0f, va("^7Chat Style: ^3%s", cStyNames[cStyCv->integer % 4]), whiteColor );

		// Button: Reset Defaults
		qboolean hReset = (qboolean)(mx >= winX + 270.0f && mx <= winX + 470.0f && my >= contentY + 200.0f && my <= contentY + 233.0f);
		SCR_DrawRoundedGlassPanel( winX + 270.0f, contentY + 200.0f, 200.0f, 33.0f, 4.0f, hReset ? borderColor : bgColor, borderColor );
		SCR_DrawVirtualString( winX + 310.0f, contentY + 211.0f, 4.2f, "^1RESET DEFAULTS", yellowCol );

	} else if ( g_rpgMenu.tab == 2 ) {
		// RANKS & XP TABLE TAB (!ranks sync)
		SCR_DrawVirtualString( winX + 25.0f, contentY, 5.4f, "^5[SERVER RANKS & ELO THRESHOLDS (!ranks)]", cyanColor );

		const char *rankList[5][3] = {
			{ "^3Youngling", "^2- 0 Elo+", "^7Starter Rank" },
			{ "^2Padawan", "^2- 1500 Elo+", "^7Apprentice Rank" },
			{ "^5Jedi Knight", "^2- 2000 Elo+", "^7Elite Knight Rank" },
			{ "^6Jedi Master", "^2- 2500 Elo+", "^7Master Rank" },
			{ "^1Grand Master", "^2- 3000 Elo+", "^7Supreme Council Rank" }
		};

		float ry = contentY + 24.0f;
		for ( int r = 0; r < 5; r++ ) {
			if ( r % 2 == 0 ) {
				vec4_t rBg = { 0.05f, 0.15f, 0.25f, 0.35f };
				SCR_FillRect( winX + 20.0f, ry, winW - 40.0f, 26.0f, rBg );
			}
			SCR_DrawVirtualString( winX + 25.0f, ry + 4.0f, 5.4f, rankList[r][0], whiteColor );
			SCR_DrawVirtualString( winX + 200.0f, ry + 4.0f, 5.4f, rankList[r][1], whiteColor );
			SCR_DrawVirtualString( winX + 360.0f, ry + 4.0f, 5.4f, rankList[r][2], whiteColor );
			ry += 30.0f;
		}

		SCR_DrawVirtualString( winX + 25.0f, ry + 16.0f, 4.6f, "^7Levels scale dynamically from 1 to 100 via ^3XP = 100 + (L-1)*150 + (L-1)^2*25^7.", whiteColor );

	} else if ( g_rpgMenu.tab == 3 ) {
		// COMMANDS DIRECTORY TAB (!help / !cmds sync)
		SCR_DrawVirtualString( winX + 25.0f, contentY, 5.4f, "^5[ALL SERVER COMMANDS DIRECTORY (!help / !cmds)]", cyanColor );

		const char *cmdList[][2] = {
			{ "/login <u > <p>", "Register or log into your account" },
			{ "!details / !myinfo", "View account credentials & display name" },
			{ "!stats / !rank / !ranks", "View stats, rank title & Elo thresholds" },
			{ "!top / !topcredits", "Global top leaderboards" },
			{ "!quests / !achievements", "Daily quests & achievement rewards" },
			{ "!shop / !buy / !sell / !inv", "Economy shop & item inventory" },
			{ "!bounty / !bet / !roll", "Bounties, duel bets & tiered roll gamble" },
			{ "!adventure / !party", "Text adventures & party studio invite" },
			{ "!admin / !freeze / !goto", "High admin controls & player management" }
		};

		float cy = contentY + 22.0f;
		for ( int c = 0; c < 9; c++ ) {
			SCR_DrawVirtualString( winX + 25.0f, cy, 4.8f, va("^3%-25s ^7%s", cmdList[c][0], cmdList[c][1]), whiteColor );
			cy += 24.0f;
		}
	}

	SCR_DrawVirtualString( winX + (winW - 160.0f) * 0.5f, winY + winH - 15.0f, 4.0f, "^7Press ^1[ESC]^7 to close Master Hub", whiteColor );
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

	SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 8.0f, bgColor, borderColor );

	float titleW = SCR_GetStringWidth( "PARTY MANAGEMENT STUDIO", 7.5f );
	SCR_DrawVirtualString( winX + (winW - titleW) * 0.5f, winY + 14.0f, 7.5f, "^5PARTY MANAGEMENT STUDIO", yellowCol );
	SCR_DrawVirtualString( winX + winW - 35.0f, winY + 10.0f, 5.0f, "^1[ESC]", yellowCol );

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

	SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 8.0f, bgColor, borderColor );

	float titleW = SCR_GetStringWidth( "ADMIN CONTROL PANEL", 7.5f );
	SCR_DrawVirtualString( winX + (winW - titleW) * 0.5f, winY + 16.0f, 7.5f, "^1ADMIN CONTROL PANEL", yellowCol );
	SCR_DrawVirtualString( winX + winW - 35.0f, winY + 10.0f, 5.0f, "^1[ESC]", yellowCol );

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
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {

	if ( cls.state != s_lastState ) {
		s_hBox = 0;
		s_hBoxFillV1 = 0;
		s_hBoxV2 = 0;
		s_hBoxFillV2 = 0;
		s_hBoxV3 = 0;
		s_hBoxFillV3 = 0;
		s_hBarBg = 0;
		s_hBarFill = 0;
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
					SCR_DrawLeaderboardOverlay();
					SCR_DrawStatsOverlay();
					SCR_DrawToastOverlay();
					SCR_DrawInspectOverlay();
					SCR_DrawBountyOverlay();
					SCR_DrawShopOverlay();
					SCR_DrawQuestOverlay();
					SCR_DrawInventoryOverlay();
					SCR_DrawAchievementsOverlay();
					SCR_DrawTopCreditsOverlay();
					SCR_DrawTopPotatoOverlay();
					SCR_DrawAdventureOverlay();
					void SCR_DrawRPGMenuOverlay( void );
					void SCR_DrawPartyStudioOverlay( void );
					void SCR_DrawAdminOverlay( void );
					SCR_DrawRPGMenuOverlay();
					SCR_DrawPartyStudioOverlay();
					SCR_DrawAdminOverlay();

					// Draw dynamic virtual mouse cursor ONLY on active interactive menus (Shop, Inventory, Adventure, RPG Menu, Party Studio, Admin)
					if ( g_rpgShop.active || g_rpgInventory.active || g_rpgAdv.active || g_rpgMenu.active || g_rpgPartyStudio.active || g_rpgAdmin.active ) {
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
	s_hBox = 0;
	s_hBoxFillV1 = 0;
	s_hBoxV2 = 0;
	s_hBoxFillV2 = 0;
	s_hBoxV3 = 0;
	s_hBoxFillV3 = 0;
	s_hBarBg = 0;
	s_hBarFill = 0;
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
	g_rpgAch.active = qfalse;
	g_rpgTopCredits.active = qfalse;
	g_rpgTopPotato.active = qfalse;
	g_rpgStats.active = qfalse;
	g_rpgBounty.active = qfalse;
	g_rpgMenu.active = qfalse;
	g_rpgPartyStudio.active = qfalse;
	g_rpgAdmin.active = qfalse;
	g_rpgSettings.active = qfalse;
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
