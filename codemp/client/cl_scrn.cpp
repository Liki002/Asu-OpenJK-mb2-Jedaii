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
cvar_t		*cg_drawQuestInv;
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
rpgQuestInvOverlay_t  g_rpgQuestInv      = {qfalse, 0, 0, {}, 0, {}};
rpgAchOverlay_t       g_rpgAch           = {qfalse, 0, {}};
rpgTopCreditsOverlay_t g_rpgTopCredits   = {qfalse, 0, {}};
rpgTopPotatoOverlay_t g_rpgTopPotato     = {qfalse, 0, {}};
rpgAdventureOverlay_t g_rpgAdv           = {qfalse, "", "", "", "", ""};
rpgPartyOverlay_t     g_rpgParty         = {qfalse, "", 0, 0, {}};




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



/*
** SCR_DrawChar
** chars are drawn at 640*480 virtual screen size with clean 0.60 width ratio
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
	aw = size * 0.60f; // Proportional 0.60 width matching font step
	ah = size;

	row = ch>>4;
	col = ch&15;

	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	re->DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow,
					   fcol + size, frow + size2,
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

	frow = row*0.0625;
	fcol = col*0.0625;

	size = 0.03125;
//	size = 0.0625;

	size2 = 0.0625;

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
		xx += size;
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
		xx += size;
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
		Com_Printf( "^3Usage: ^7rpg_hud_style <classic | bottom | 0 | 1>\n" );
		return;
	}
	const char *style = Cmd_Argv( 1 );
	if ( !Q_stricmp( style, "bottom" ) || !Q_stricmp( style, "minimal" ) || !Q_stricmp( style, "1" ) ) {
		Cvar_Set( "cg_rpg_style", "1" );
		Com_Printf( "^2RPG HUD style set to BOTTOM SLEEK BAR (Style 1)\n" );
	} else {
		Cvar_Set( "cg_rpg_style", "0" );
		Com_Printf( "^2RPG HUD style set to CLASSIC GLASS PANEL (Style 0)\n" );
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
	cg_drawQuestInv = Cvar_Get ("cg_drawQuestInv", "0", 0);
	cg_drawAch = Cvar_Get ("cg_drawAch", "0", 0);
	cg_drawTopCredits = Cvar_Get ("cg_drawTopCredits", "0", 0);
	cg_drawTopPotato = Cvar_Get ("cg_drawTopPotato", "0", 0);
	cg_drawAdv = Cvar_Get ("cg_drawAdv", "0", 0);


	Cmd_AddCommand( "rpg_hud_style", SCR_RPGHUDStyle_f, "Select RPG HUD style: classic (0) or bottom (1)" );
	Cmd_AddCommand( "rpg_hud_pos", SCR_RPGHUDPos_f, "Position RPG HUD: left, right, bottomright, bottomleft, bottomcenter" );

	scr_initialized = qtrue;
}

/*
==================
SCR_FillRoundedRect

Draws a rectangle with smooth rounded corners in 640x480 virtual coordinates
==================
*/
/*
==================
SCR_DrawRoundedGlassPanel

Renders a high-tech translucent glass panel with smooth rounded corners and a crisp 1px border.
==================
*/
static void SCR_DrawRoundedGlassPanel( float x, float y, float w, float h, float r, const float *bgColor, const float *borderColor ) {
	if ( w <= 0 || h <= 0 ) return;
	if ( r < 1.0f ) r = 1.0f;

	// 1. Fill translucent rounded body
	SCR_FillRect( x + r, y, w - 2.0f * r, h, bgColor );
	SCR_FillRect( x, y + r, r, h - 2.0f * r, bgColor );
	SCR_FillRect( x + w - r, y + r, r, h - 2.0f * r, bgColor );

	// Smooth corner fill caps
	float hr = r * 0.5f;
	SCR_FillRect( x + hr, y + hr, hr, hr, bgColor );
	SCR_FillRect( x + w - r + (r - hr), y + hr, hr, hr, bgColor );
	SCR_FillRect( x + hr, y + h - r + (r - hr), hr, hr, bgColor );
	SCR_FillRect( x + w - r + (r - hr), y + h - r + (r - hr), hr, hr, bgColor );

	// 2. Draw crisp 1px rounded border line
	if ( borderColor ) {
		// Straight edges
		SCR_FillRect( x + r, y, w - 2.0f * r, 1.0f, borderColor );                     // Top
		SCR_FillRect( x + r, y + h - 1.0f, w - 2.0f * r, 1.0f, borderColor );         // Bottom
		SCR_FillRect( x, y + r, 1.0f, h - 2.0f * r, borderColor );                     // Left
		SCR_FillRect( x + w - 1.0f, y + r, 1.0f, h - 2.0f * r, borderColor );         // Right

		// Smooth 1px corner stepped border lines
		SCR_FillRect( x + 1.0f, y + 1.0f, r - 1.0f, 1.0f, borderColor );
		SCR_FillRect( x + 1.0f, y + 1.0f, 1.0f, r - 1.0f, borderColor );

		SCR_FillRect( x + w - r, y + 1.0f, r - 1.0f, 1.0f, borderColor );
		SCR_FillRect( x + w - 2.0f, y + 1.0f, 1.0f, r - 1.0f, borderColor );

		SCR_FillRect( x + 1.0f, y + h - 2.0f, r - 1.0f, 1.0f, borderColor );
		SCR_FillRect( x + 1.0f, y + h - r, 1.0f, r - 1.0f, borderColor );

		SCR_FillRect( x + w - r, y + h - 2.0f, r - 1.0f, 1.0f, borderColor );
		SCR_FillRect( x + w - 2.0f, y + h - r, 1.0f, r - 1.0f, borderColor );
	}
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

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			Com_Memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
			re->SetColor( color );
			s += 2;
			continue;
		}
		SCR_DrawChar( (int)xx, (int)y, charSize, *s );
		xx += (charSize * 0.60f);
		s++;
	}
	re->SetColor( NULL );
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
static qhandle_t s_hBarBg = 0;
static qhandle_t s_hBarFill = 0;
static qhandle_t s_hAvatar = 0;
static qhandle_t s_hAvatarFrame = 0;
static qhandle_t s_hModalBg = 0;
static qhandle_t s_hWantedBg = 0;
static qhandle_t s_hShopBg = 0;
static qhandle_t s_hQuestBg = 0;
static qhandle_t s_hAchBg = 0;
static qhandle_t s_hTopBg = 0;
static qhandle_t s_hAdvBg = 0;
static qhandle_t s_hPotatoPic = 0;

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
		s_hBarBg       = re->RegisterShader( "gfx/rpg_hud/bar_bg" );
		s_hBarFill     = re->RegisterShader( "gfx/rpg_hud/bar_fill" );
		s_hAvatar      = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
		s_hAvatarFrame = re->RegisterShader( "gfx/rpg_hud/avatar_frame" );
		s_hModalBg     = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );
	}




	int style = cg_rpg_style ? cg_rpg_style->integer : 0;
	cvar_t *clName = Cvar_Get( "name", "Padawan", 0 );
	const char *playerName = (cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : (clName ? clName->string : "Player");
	const char *rankTitle = (cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : "Padawan";

	float nameW = (float)SCR_Strlen( playerName ) * 5.2f * 0.60f;
	float titleW = (float)SCR_Strlen( rankTitle ) * 4.3f * 0.60f;
	float maxStrW = (nameW > titleW) ? nameW : titleW;
	float panelW = (style == 1) ? (maxStrW + 115.0f) : (maxStrW + 55.0f);
	if ( panelW < 160.0f ) panelW = 160.0f;

	float panelH = 46.0f;

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

	int level = cg_rpg_level ? cg_rpg_level->integer : 1;
	int fr = cg_rpg_fr ? cg_rpg_fr->integer : 1000;
	int xp = cg_rpg_xp ? cg_rpg_xp->integer : 0;
	int xpMax = (cg_rpg_xp_max && cg_rpg_xp_max->integer > 0) ? cg_rpg_xp_max->integer : 1000;

	if ( xp < 0 ) xp = 0;
	if ( xp > xpMax ) xp = xpMax;

	if ( s_visualXP < 0.0f ) {
		s_visualXP = (float)xp;
	} else {
		float diff = (float)xp - s_visualXP;
		if ( fabsf( diff ) > 0.1f ) {
			s_visualXP += diff * 0.08f;
		} else {
			s_visualXP = (float)xp;
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
		// Avatar silhouette (Fitted inside frame)
		float avatarX = panelX + 4.0f;
		float avatarY = panelY - 1.0f;
		float avatarSize = 26.0f;

		if ( s_hAvatar && s_hAvatarFrame ) {
			SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatarFrame );
			SCR_DrawPic( avatarX + 2.0f, avatarY + 2.0f, avatarSize - 4.0f, avatarSize - 4.0f, s_hAvatar );
		} else if ( s_hAvatar ) {
			SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatar );
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

		// Line 2: Rank Title & Force Rating ELO
		char rankStr[128];
		Com_sprintf( rankStr, sizeof(rankStr), "^3%s ^7|^2 %d", rankTitle, fr );
		SCR_DrawVirtualString( textX, panelY + 13.0f, 4.3f, rankStr, whiteColor );

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
		char xpText[64];
		Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", (int)s_visualXP, xpMax );
		float textWidthPixels = (strlen(xpText) * 3.8f * 0.60f);
		float xpTextX = barX + barW - textWidthPixels - 3.0f;
		if ( xpTextX < barX + 3.0f ) xpTextX = barX + 3.0f;
		SCR_DrawVirtualString( xpTextX, barY - 10.0f, 3.8f, xpText, whiteColor );
		return;
	}

	// ========================================================
	// STYLE 0: CLASSIC GLASS PANEL CARD (HD TGA Card)
	// ========================================================
	if ( s_hBox ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, s_hBox );
	} else {
		vec4_t bgColor     = { 0.02f, 0.05f, 0.10f, 0.20f };
		vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.40f };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, bgColor, borderColor );
	}

	// Avatar (Fitted inside circular frame)
	float avatarX = panelX + 5.0f;
	float avatarY = panelY + 5.0f;
	float avatarSize = 22.0f;

	if ( s_hAvatar && s_hAvatarFrame ) {
		SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatarFrame );
		SCR_DrawPic( avatarX + 2.0f, avatarY + 2.0f, avatarSize - 4.0f, avatarSize - 4.0f, s_hAvatar );
	} else if ( s_hAvatar ) {
		SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, s_hAvatar );
	} else {
		float cx = avatarX + avatarSize * 0.5f;
		float cy = avatarY + avatarSize * 0.5f;
		SCR_DrawJediVectorEmblem( cx, cy, avatarSize * 0.5f );
	}

	// Level Badge
	char levelStr[32];
	Com_sprintf( levelStr, sizeof(levelStr), "^3Lv %d", level );
	float levelX = avatarX;
	float levelY = avatarY + avatarSize + 2.0f;
	SCR_DrawVirtualString( levelX, levelY, 4.2f, levelStr, whiteColor );

	// Right Content Column
	float textX = avatarX + avatarSize + 6.0f;

	// Line 1: Player Name
	char nameStr[128];
	Com_sprintf( nameStr, sizeof(nameStr), "^7%s", playerName );
	SCR_DrawVirtualString( textX, panelY + 4.0f, 5.2f, nameStr, whiteColor );

	// Line 2: Rank Title & Force Rating ELO
	char rankStr[128];
	Com_sprintf( rankStr, sizeof(rankStr), "^3%s ^7|^2 %d", rankTitle, fr );
	SCR_DrawVirtualString( textX, panelY + 15.0f, 4.3f, rankStr, whiteColor );

	// Line 3: XP Bar
	float barX = textX;
	float barY = panelY + 28.0f;
	float barW = panelX + panelW - barX - 6.0f;
	float barH = 7.0f;

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

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", (int)s_visualXP, xpMax );

	float textWidthPixels = (strlen(xpText) * 3.8f * 0.60f);
	float xpTextX = barX + barW - textWidthPixels - 3.0f;
	if ( xpTextX < barX + 3.0f ) xpTextX = barX + 3.0f;
	SCR_DrawVirtualString( xpTextX, barY + 1.5f, 3.8f, xpText, whiteColor );
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

	// Modal Window Dimensions (Expanded 480x320 centered modal)
	float winX = 80.0f;
	float winY = 65.0f;
	float winW = 480.0f;
	float winH = 320.0f;

	if ( s_hModalBg ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hModalBg );
	} else {
		vec4_t bgColor     = { 0.03f, 0.06f, 0.12f, 0.88f };
		vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );

		vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.88f };
		SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 26.0f, 3.0f, headerBg, NULL );
	}

	// Title
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( winX + 140.0f, winY + 8.0f, 7.2f, "^3TOP RANKED DUELISTS", yellowCol );

	// Close Button instruction
	SCR_DrawVirtualString( winX + winW - 65.0f, winY + 8.0f, 5.8f, "^1[ESC]", yellowCol );

	// Column Headers Divider line
	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.70f };
	float colY = winY + 36.0f;
	SCR_FillRect( winX + 16.0f, colY + 16.0f, winW - 32.0f, 1.0f, divColor );

	// Column Headers: # | PLAYER NAME | LVL | RANK TITLE | FR ELO
	SCR_DrawVirtualString( winX + 16.0f, colY, 5.8f, "^5#", whiteColor );
	SCR_DrawVirtualString( winX + 48.0f, colY, 5.8f, "^5PLAYER NAME", whiteColor );
	SCR_DrawVirtualString( winX + 230.0f, colY, 5.8f, "^5LVL", whiteColor );
	SCR_DrawVirtualString( winX + 280.0f, colY, 5.8f, "^5RANK TITLE", whiteColor );
	SCR_DrawVirtualString( winX + 400.0f, colY, 5.8f, "^5FR ELO", whiteColor );

	// Render Rows
	float rowStartY = colY + 20.0f;
	float rowHeight = 23.0f;

	for ( int i = 0; i < 10; i++ ) {
		float currentY = rowStartY + (i * rowHeight);

		// Alternating row background highlight
		if ( i % 2 == 0 ) {
			vec4_t rowBg = { 0.05f, 0.12f, 0.25f, 0.35f };
			SCR_FillRect( winX + 16.0f, currentY, winW - 32.0f, rowHeight - 2.0f, rowBg );
		}

		if ( i < g_topLeaderboardCount ) {
			topLeaderboardEntry_t *e = &g_topLeaderboard[i];

			// Rank position #
			char numStr[16];
			Com_sprintf( numStr, sizeof(numStr), (i < 3) ? "^3#%d" : "^7#%d", i + 1 );
			SCR_DrawVirtualString( winX + 16.0f, currentY + 2.0f, 5.5f, numStr, whiteColor );

			// Player Name (up to 30 characters)
			char pNameStr[64];
			Com_sprintf( pNameStr, sizeof(pNameStr), "^7%.30s", e->displayName );
			SCR_DrawVirtualString( winX + 48.0f, currentY + 2.0f, 5.5f, pNameStr, whiteColor );

			// Level
			char lvlStr[16];
			Com_sprintf( lvlStr, sizeof(lvlStr), "^3%d", e->level );
			SCR_DrawVirtualString( winX + 230.0f, currentY + 2.0f, 5.5f, lvlStr, whiteColor );

			// Rank title (Full titles up to 20 characters: "Grand Master" fits cleanly!)
			char titleStr[32];
			Com_sprintf( titleStr, sizeof(titleStr), "^3%.20s", e->rankTitle );
			SCR_DrawVirtualString( winX + 280.0f, currentY + 2.0f, 5.2f, titleStr, whiteColor );

			// FR ELO
			char frStr[32];
			Com_sprintf( frStr, sizeof(frStr), "^2%d", e->fr );
			SCR_DrawVirtualString( winX + 400.0f, currentY + 2.0f, 5.5f, frStr, whiteColor );
		}
	}

	// Footer instruction
	SCR_DrawVirtualString( winX + 120.0f, winY + winH - 15.0f, 5.2f, "^7Press ^3F8^7, ^3ESC^7, or type ^3!top^7 to close", whiteColor );
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

	// Modal Window Dimensions (Centered 490x270 card layout)
	float winW = 490.0f;
	float winH = 270.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 105.0f;

	vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.85f };

	if ( !s_hBox ) s_hBox = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
	if ( s_hBox ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hBox );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 22.0f, 3.0f, headerBg, NULL );

	// Title
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( winX + 130.0f, winY + 6.0f, 6.0f, "^3CHARACTER STATISTICS", yellowCol );

	// Close Button instruction
	SCR_DrawVirtualString( winX + winW - 55.0f, winY + 6.0f, 5.0f, "^1[ESC]", yellowCol );

	// LEFT COLUMN - Profile Picture & Title card
	float avatarX = winX + 15.0f;
	float avatarY = winY + 36.0f;
	float avatarSize = 54.0f;

	vec4_t avBg = { 0.06f, 0.12f, 0.25f, 0.50f };
	SCR_DrawRoundedGlassPanel( avatarX, avatarY, avatarSize, avatarSize, 4.0f, avBg, borderColor );

	qhandle_t hAv = s_hAvatar;
	if ( !hAv ) hAv = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
	if ( hAv ) {
		SCR_DrawPic( avatarX + 2.0f, avatarY + 2.0f, avatarSize - 4.0f, avatarSize - 4.0f, hAv );
	}

	// Level Badge
	char lvlStr[32];
	Com_sprintf( lvlStr, sizeof(lvlStr), "^3Lv %d", g_rpgStats.level );
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 6.0f, 5.0f, lvlStr, whiteColor );

	// Display Name
	char dName[128];
	Com_sprintf( dName, sizeof(dName), "^7%s", g_rpgStats.name[0] ? g_rpgStats.name : "Player" );
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 20.0f, 4.8f, dName, whiteColor );

	// Rank Title
	char rTitle[128];
	Com_sprintf( rTitle, sizeof(rTitle), "^3%s", g_rpgStats.rankTitle[0] ? g_rpgStats.rankTitle : "Padawan" );
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 32.0f, 4.5f, rTitle, whiteColor );

	// Divider line between columns
	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 130.0f, winY + 34.0f, 1.0f, winH - 52.0f, divColor );

	// RIGHT COLUMN - Statistics Rows
	float rightX = winX + 140.0f;
	float rightY = winY + 36.0f;

	// Row 1: XP Progress & Credits
	SCR_DrawVirtualString( rightX, rightY, 4.6f, va( "^5Total XP: ^2%d XP  ^7|  ^5Credits: ^3%d CR", g_rpgStats.xp, g_rpgStats.credits ), whiteColor );

	// Row 2: Force Rating (ELO)
	SCR_DrawVirtualString( rightX, rightY + 17.0f, 4.6f, va( "^5Force Rating: ^7%d ELO", g_rpgStats.elo ), whiteColor );

	// Row 3: Wins / Losses
	float wlRatio = g_rpgStats.losses > 0 ? (float)g_rpgStats.wins / (float)g_rpgStats.losses : (float)g_rpgStats.wins;
	SCR_DrawVirtualString( rightX, rightY + 34.0f, 4.6f, va( "^5Wins / Losses: ^2%d^7/^1%d ^5(W/L: %.2f)", g_rpgStats.wins, g_rpgStats.losses, wlRatio ), whiteColor );

	// Row 4: Kills / Deaths
	float kdRatio = g_rpgStats.deaths > 0 ? (float)g_rpgStats.kills / (float)g_rpgStats.deaths : (float)g_rpgStats.kills;
	SCR_DrawVirtualString( rightX, rightY + 51.0f, 4.6f, va( "^5Kills / Deaths: ^2%d^7/^1%d ^5(K/D: %.2f)", g_rpgStats.kills, g_rpgStats.deaths, kdRatio ), whiteColor );

	// Row 5: Highest Streak
	SCR_DrawVirtualString( rightX, rightY + 68.0f, 4.6f, va( "^5Highest Streak: ^3%d wins", g_rpgStats.highestStreak ), whiteColor );

	// Row 6: Favorite Weapon
	SCR_DrawVirtualString( rightX, rightY + 85.0f, 4.6f, va( "^5Fav Weapon: ^7%s", g_rpgStats.favWeapon[0] ? g_rpgStats.favWeapon : "Lightsaber" ), whiteColor );

	// Row 7: Trivia Wins
	SCR_DrawVirtualString( rightX, rightY + 102.0f, 4.6f, va( "^5Trivia Wins: ^7%d Wins", g_rpgStats.triviaWins ), whiteColor );

	// Row 8: Main Rival
	char rivalStr[128];
	if ( g_rpgStats.topRivalCount > 0 ) {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^5Top Rival: ^1%s ^5(%d duels)", g_rpgStats.topRivalName, g_rpgStats.topRivalCount );
	} else {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^5Top Rival: ^7None" );
	}
	SCR_DrawVirtualString( rightX, rightY + 119.0f, 4.6f, rivalStr, whiteColor );

	// Footer instruction
	SCR_DrawVirtualString( winX + 110.0f, winY + winH - 12.0f, 4.5f, "^7Press ^3F8^7, ^3ESC^7, or type ^3!stats^7 to close", whiteColor );
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

	if ( !s_hBox ) s_hBox = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
	if ( s_hBox ) {
		re->SetColor( whiteA );
		re->DrawStretchPic( panelX, panelY, panelW, panelH, 0, 0, 1, 1, s_hBox );
		re->SetColor( NULL );
	} else {
		// Panel background (dark navy glass) fallback
		vec4_t bgColor   = { 0.04f, 0.07f, 0.14f, 0.88f * alpha };
		re->SetColor( bgColor );
		re->DrawStretchPic( panelX, panelY, panelW, panelH, 0, 0, 0, 0, cls.whiteShader );
		re->SetColor( NULL );

		// Outer border fallback
		vec4_t borderColor = { 0.20f, 0.50f, 0.80f, 0.55f * alpha };
		SCR_FillRect( panelX,              panelY,              panelW, 1.0f,   borderColor );
		SCR_FillRect( panelX,              panelY + panelH - 1, panelW, 1.0f,   borderColor );
		SCR_FillRect( panelX,              panelY,              1.0f,   panelH, borderColor );
		SCR_FillRect( panelX + panelW - 1, panelY,              1.0f,   panelH, borderColor );
	}

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
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP ^7(^2%d HP ^7| ^5%d BP^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp, g_rpgToast.health, g_rpgToast.bp );
		} else if ( g_rpgToast.health > 0 ) {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP ^7(^2%d HP^7)", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp, g_rpgToast.health );
		} else {
			Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp );
		}

	} else {
		Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^1%d ^7| ^6+%d CR", g_rpgToast.eloDelta, g_rpgToast.credits );
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
	Com_sprintf( statStr, sizeof( statStr ), "^5Lv %d  ^7|  ^2%d FR", g_rpgInspect.level, g_rpgInspect.fr );

	float fontSize = 4.2f;
	float w = (float)SCR_Strlen( statStr ) * fontSize * 0.60f;
	float x = 320.0f - w * 0.5f;
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

	float winW = 420.0f;
	float winH = 240.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 120.0f;

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
	}

	// Header background
	vec4_t headerBg;
	if ( g_rpgBounty.isWanted ) {
		headerBg[0] = 0.40f; headerBg[1] = 0.08f; headerBg[2] = 0.08f; headerBg[3] = 0.88f;
	} else {
		headerBg[0] = 0.08f; headerBg[1] = 0.18f; headerBg[2] = 0.35f; headerBg[3] = 0.88f;
	}
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 22.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	if ( g_rpgBounty.isWanted ) {
		SCR_DrawVirtualString( winX + 100.0f, winY + 6.0f, 6.0f, "^1WANTED TARGETS ^7(Duel Streaks)", yellowCol );
	} else {
		SCR_DrawVirtualString( winX + 100.0f, winY + 6.0f, 6.0f, "^3ACTIVE BOUNTIES ^7(Online)", yellowCol );
	}

	SCR_DrawVirtualString( winX + winW - 55.0f, winY + 6.0f, 5.0f, "^1[ESC]", yellowCol );

	// Table Headers
	float startY = winY + 34.0f;
	SCR_DrawVirtualString( winX + 16.0f, startY, 4.8f, "^5#", yellowCol );
	SCR_DrawVirtualString( winX + 45.0f, startY, 4.8f, "^5PLAYER NAME", yellowCol );
	if ( g_rpgBounty.isWanted ) {
		SCR_DrawVirtualString( winX + 260.0f, startY, 4.8f, "^5STREAK", yellowCol );
		SCR_DrawVirtualString( winX + 335.0f, startY, 4.8f, "^5BOUNTY", yellowCol );
	} else {
		SCR_DrawVirtualString( winX + 295.0f, startY, 4.8f, "^5BOUNTY REWARD", yellowCol );
	}

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY + 14.0f, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgBounty.count == 0 ) {
		SCR_DrawVirtualString( winX + 130.0f, startY + 40.0f, 5.2f, "^7No active targets found.", whiteColor );
	} else {
		float rowY = startY + 20.0f;
		for ( int i = 0; i < g_rpgBounty.count && i < 8; i++ ) {
			bountyEntry_t *e = &g_rpgBounty.entries[i];

			// Highlight top 1
			if ( i == 0 ) {
				vec4_t topBg = { 0.80f, 0.60f, 0.10f, 0.15f };
				SCR_FillRect( winX + 10.0f, rowY - 1.0f, winW - 20.0f, 16.0f, topBg );
			}

			// Rank
			SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%d", e->rank ), whiteColor );

			// Name (full long name support)
			char nameFormatted[64];
			Com_sprintf( nameFormatted, sizeof( nameFormatted ), "^7%.28s", e->name );
			SCR_DrawVirtualString( winX + 45.0f, rowY, 4.8f, nameFormatted, whiteColor );

			// Values
			if ( g_rpgBounty.isWanted ) {
				SCR_DrawVirtualString( winX + 260.0f, rowY, 4.8f, va( "^5%d wins", e->streak ), whiteColor );
				if ( e->bounty > 0 ) {
					SCR_DrawVirtualString( winX + 335.0f, rowY, 4.8f, va( "^3%d CR", e->bounty ), whiteColor );
				} else {
					SCR_DrawVirtualString( winX + 335.0f, rowY, 4.8f, "^7-", whiteColor );
				}
			} else {
				SCR_DrawVirtualString( winX + 295.0f, rowY, 4.8f, va( "^3%d Credits", e->bounty ), whiteColor );
			}

			rowY += 18.0f;
		}
	}

	// Footer instruction
	const char *closeStr = g_rpgBounty.isWanted ? "^7Press ^3F8^7, ^3ESC^7, or type ^3!wanted^7 to close" : "^7Press ^3F8^7, ^3ESC^7, or type ^3!bountylist^7 to close";
	SCR_DrawVirtualString( winX + 80.0f, winY + winH - 12.0f, 4.5f, closeStr, whiteColor );
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

	// Calculate dynamic width based on item descriptions & keys
	float maxStrWidth = 140.0f;
	for ( int i = 0; i < g_rpgShop.count; i++ ) {
		rpgShopItem_t *item = &g_rpgShop.items[i];
		float strW = (float)SCR_Strlen( item->display ) * 4.8f * 0.60f;
		if ( strW > maxStrWidth ) maxStrWidth = strW;
	}

	float winW = maxStrWidth + 240.0f;
	if ( winW < 460.0f ) winW = 460.0f;
	if ( winW > 620.0f ) winW = 620.0f;

	float winH = 260.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 110.0f;

	if ( s_hShopBg <= 0 && re && re->RegisterShader ) {
		s_hShopBg = re->RegisterShader( "gfx/rpg_hud/shop_bg" );
	}

	if ( s_hShopBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hShopBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.10f, 0.75f, 0.95f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}


	vec4_t headerBg = { 0.08f, 0.20f, 0.38f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 6.0f, "^5RANKED SHOP", yellowCol );
	char crStr[64];
	Com_sprintf( crStr, sizeof( crStr ), "^7Credits: ^5%d CR", g_rpgShop.credits );
	SCR_DrawVirtualString( winX + winW - 140.0f, winY + 7.0f, 5.5f, crStr, whiteColor );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	// Headers
	float startY = winY + 36.0f;
	SCR_DrawVirtualString( winX + 16.0f, startY, 4.8f, "^5KEY", yellowCol );
	SCR_DrawVirtualString( winX + 90.0f, startY, 4.8f, "^5DESCRIPTION", yellowCol );
	SCR_DrawVirtualString( winX + winW - 130.0f, startY, 4.8f, "^5PRICE", yellowCol );
	SCR_DrawVirtualString( winX + winW - 65.0f, startY, 4.8f, "^5SELL", yellowCol );

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY + 14.0f, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgShop.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, startY + 40.0f, 5.2f, "^7No shop items available.", whiteColor );
	} else {
		int maxVisible = 7;
		int startIdx = g_rpgShop.scroll;
		if ( startIdx < 0 ) startIdx = 0;

		float rowY = startY + 20.0f;
		for ( int i = startIdx; i < g_rpgShop.count && (i - startIdx) < maxVisible; i++ ) {
			rpgShopItem_t *item = &g_rpgShop.items[i];

			// Key
			SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%s", item->key ), whiteColor );

			// Description
			SCR_DrawVirtualString( winX + 90.0f, rowY, 4.8f, va( "^7%s", item->display ), whiteColor );

			// Price & Sell
			SCR_DrawVirtualString( winX + winW - 130.0f, rowY, 4.8f, va( "^5%d cr", item->price ), whiteColor );
			SCR_DrawVirtualString( winX + winW - 65.0f, rowY, 4.8f, va( "^3%d", item->sellBack ), whiteColor );

			rowY += 18.0f;
		}

		// Draw Vertical Scroll Bar Indicator if more items than fit
		if ( g_rpgShop.count > maxVisible ) {
			float barX = winX + winW - 12.0f;
			float barY = startY + 20.0f;
			float barH = maxVisible * 18.0f;
			vec4_t scrollBg = { 0.05f, 0.10f, 0.20f, 0.60f };
			vec4_t scrollThumb = { 0.10f, 0.70f, 1.00f, 0.85f };
			SCR_FillRect( barX, barY, 4.0f, barH, scrollBg );

			float calcH = barH * ((float)maxVisible / (float)g_rpgShop.count);
			float thumbH = (calcH < 12.0f) ? 12.0f : calcH;

			float maxScroll = (float)(g_rpgShop.count - maxVisible);
			float thumbY = barY + (barH - thumbH) * ((float)startIdx / maxScroll);
			SCR_FillRect( barX, thumbY, 4.0f, thumbH, scrollThumb );
		}
	}

	SCR_DrawVirtualString( winX + 20.0f, winY + winH - 12.0f, 4.5f, "^7Scroll ^3[Mouse Wheel]^7 | ^3!buy <key>^7 to purchase | ^3!sell <key>^7 to sell | ^1[ESC]", whiteColor );
}



/*
==================
SCR_DrawQuestInvOverlay

Combined Tabbed Modal for !quests and !inventory
==================
*/
void SCR_DrawQuestInvOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawQuestInv || !cg_drawQuestInv->integer ) return;
	if ( !g_rpgQuestInv.active ) return;

	float winW = 500.0f;
	float winH = 260.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 110.0f;

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


	// Header Tabs
	vec4_t tabBgActive = { 0.10f, 0.35f, 0.25f, 0.90f };
	vec4_t tabBgInactive = { 0.05f, 0.10f, 0.18f, 0.60f };
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	qboolean isQuestsTab = ( g_rpgQuestInv.activeTab == 0 ) ? qtrue : qfalse;


	SCR_DrawRoundedGlassPanel( winX + 6.0f, winY + 4.0f, 140.0f, 22.0f, 3.0f, isQuestsTab ? tabBgActive : tabBgInactive, NULL );
	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 5.2f, isQuestsTab ? "^2[1] DAILY QUESTS" : "^7[1] DAILY QUESTS", whiteColor );

	SCR_DrawRoundedGlassPanel( winX + 152.0f, winY + 4.0f, 130.0f, 22.0f, 3.0f, !isQuestsTab ? tabBgActive : tabBgInactive, NULL );
	SCR_DrawVirtualString( winX + 162.0f, winY + 7.0f, 5.2f, !isQuestsTab ? "^2[2] INVENTORY" : "^7[2] INVENTORY", whiteColor );

	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	float startY = winY + 36.0f;
	vec4_t divColor = { 0.20f, 0.80f, 0.50f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY, winW - 20.0f, 1.0f, divColor );

	if ( isQuestsTab ) {
		// Daily Quests Content
		if ( g_rpgQuestInv.questCount == 0 ) {
			SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, startY + 40.0f, 5.2f, "^7No daily quests available.", whiteColor );
		} else {
			float rowY = startY + 12.0f;
			for ( int i = 0; i < g_rpgQuestInv.questCount && i < 7; i++ ) {
				rpgQuestEntry_t *q = &g_rpgQuestInv.quests[i];

				if ( q->done ) {
					SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%d. ^2[DONE] ^7%s", q->id, q->desc ), whiteColor );
					SCR_DrawVirtualString( winX + winW - 100.0f, rowY, 4.8f, va( "^6[%s]", q->mode ), whiteColor );
				} else {
					SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%d. ^7%s", q->id, q->desc ), whiteColor );
					SCR_DrawVirtualString( winX + 270.0f, rowY, 4.8f, va( "^5[%d/%d]", q->prog, q->goal ), whiteColor );
					SCR_DrawVirtualString( winX + 340.0f, rowY, 4.8f, va( "^3+%d CR ^2+%d FR", q->rewardCr, q->rewardFr ), whiteColor );
					SCR_DrawVirtualString( winX + winW - 75.0f, rowY, 4.8f, va( "^6[%s]", q->mode ), whiteColor );
				}
				rowY += 24.0f;
			}
		}
	} else {
		// Inventory Content
		if ( g_rpgQuestInv.invCount == 0 ) {
			SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, startY + 40.0f, 5.2f, "^7Inventory is empty.", whiteColor );
		} else {
			float rowY = startY + 12.0f;
			for ( int i = 0; i < g_rpgQuestInv.invCount && i < 8; i++ ) {
				rpgInvEntry_t *item = &g_rpgQuestInv.inv[i];
				SCR_DrawVirtualString( winX + 24.0f, rowY, 5.0f, va( "^7%s", item->display ), whiteColor );
				SCR_DrawVirtualString( winX + 220.0f, rowY, 5.0f, va( "^3(!use %s)", item->key ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 90.0f, rowY, 5.0f, va( "^5x%d", item->qty ), whiteColor );
				rowY += 20.0f;
			}
		}
	}

	SCR_DrawVirtualString( winX + 20.0f, winY + winH - 12.0f, 4.5f, "^7Press ^31^7 or ^32^7 to switch tabs  |  Type ^3!use <item>^7  |  ^1[ESC]^7 to close", whiteColor );
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

	float winW = 440.0f;
	float winH = 260.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 110.0f;

	if ( s_hAchBg <= 0 && re && re->RegisterShader ) {
		s_hAchBg = re->RegisterShader( "gfx/rpg_hud/ach_bg" );
	}

	if ( s_hAchBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hAchBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.85f, 0.65f, 0.10f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}


	vec4_t headerBg = { 0.35f, 0.25f, 0.05f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 6.0f, "^3ACHIEVEMENTS & BADGES", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	float startY = winY + 36.0f;
	vec4_t divColor = { 0.85f, 0.65f, 0.10f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgAch.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, startY + 40.0f, 5.2f, "^7No achievements loaded.", whiteColor );
	} else {
		float rowY = startY + 12.0f;
		for ( int i = 0; i < g_rpgAch.count && i < 9; i++ ) {
			rpgAchEntry_t *e = &g_rpgAch.entries[i];

			if ( e->unlocked ) {
				SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^2[UNLOCKED] ^7%s", e->name ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 90.0f, rowY, 4.8f, va( "^5+%d CR", e->rewardCr ), whiteColor );
			} else {
				SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^7[LOCKED] %s", e->name ), whiteColor );
				SCR_DrawVirtualString( winX + winW - 90.0f, rowY, 4.8f, va( "^3+%d CR", e->rewardCr ), whiteColor );
			}
			rowY += 18.0f;
		}
	}

	SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, winY + winH - 12.0f, 4.5f, "^7Press ^1[ESC]^7 to close", whiteColor );
}


/*
==================
SCR_DrawTopCreditsOverlay

Top Credits Leaderboard Card
==================
*/
void SCR_DrawTopCreditsOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawTopCredits || !cg_drawTopCredits->integer ) return;
	if ( !g_rpgTopCredits.active ) return;

	float maxNameW = 140.0f;
	for ( int i = 0; i < g_rpgTopCredits.count; i++ ) {
		float strW = (float)SCR_Strlen( g_rpgTopCredits.entries[i].name ) * 4.8f * 0.60f;
		if ( strW > maxNameW ) maxNameW = strW;
	}

	float winW = maxNameW + 180.0f;
	if ( winW < 380.0f ) winW = 380.0f;

	float winH = 240.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 120.0f;

	if ( s_hTopBg <= 0 && re && re->RegisterShader ) {
		s_hTopBg = re->RegisterShader( "gfx/rpg_hud/top_bg" );
	}

	if ( s_hTopBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hTopBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 0.20f, 0.85f, 0.40f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}


	vec4_t headerBg = { 0.05f, 0.30f, 0.12f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 6.0f, "^2TOP WEALTHIEST DUELISTS", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	float startY = winY + 36.0f;
	SCR_DrawVirtualString( winX + 16.0f, startY, 4.8f, "^5#", yellowCol );
	SCR_DrawVirtualString( winX + 45.0f, startY, 4.8f, "^5DUELIST NAME", yellowCol );
	SCR_DrawVirtualString( winX + winW - 110.0f, startY, 4.8f, "^5CREDITS", yellowCol );

	vec4_t divColor = { 0.20f, 0.85f, 0.40f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY + 14.0f, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgTopCredits.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, startY + 40.0f, 5.2f, "^7No top credits data.", whiteColor );
	} else {
		float rowY = startY + 20.0f;
		for ( int i = 0; i < g_rpgTopCredits.count && i < 10; i++ ) {
			topCreditsEntry_t *e = &g_rpgTopCredits.entries[i];

			SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%d", e->rank ), whiteColor );
			SCR_DrawVirtualString( winX + 45.0f, rowY, 4.8f, va( "^7%s", e->name ), whiteColor );
			SCR_DrawVirtualString( winX + winW - 110.0f, rowY, 4.8f, va( "^5%d CR", e->credits ), whiteColor );

			rowY += 17.0f;
		}
	}

	SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, winY + winH - 12.0f, 4.5f, "^7Press ^1[ESC]^7 to close", whiteColor );
}


/*
==================
SCR_DrawTopPotatoOverlay

Top Hot Potato Leaderboard Card
==================
*/
void SCR_DrawTopPotatoOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_drawTopPotato || !cg_drawTopPotato->integer ) return;
	if ( !g_rpgTopPotato.active ) return;

	float maxNameW = 140.0f;
	for ( int i = 0; i < g_rpgTopPotato.count; i++ ) {
		float strW = (float)SCR_Strlen( g_rpgTopPotato.entries[i].name ) * 4.8f * 0.60f;
		if ( strW > maxNameW ) maxNameW = strW;
	}

	float winW = maxNameW + 180.0f;
	if ( winW < 380.0f ) winW = 380.0f;

	float winH = 240.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 120.0f;

	if ( s_hTopBg <= 0 && re && re->RegisterShader ) {
		s_hTopBg = re->RegisterShader( "gfx/rpg_hud/top_bg" );
	}

	if ( s_hTopBg > 0 ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hTopBg );
	} else {
		vec4_t bgColor = { 0.03f, 0.06f, 0.12f, 0.90f };
		vec4_t borderColor = { 1.00f, 0.45f, 0.10f, 0.85f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t headerBg = { 0.40f, 0.15f, 0.05f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 6.0f, "^1TOP HOT POTATO SURVIVORS", yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	float startY = winY + 36.0f;
	SCR_DrawVirtualString( winX + 16.0f, startY, 4.8f, "^5#", yellowCol );
	SCR_DrawVirtualString( winX + 45.0f, startY, 4.8f, "^5PLAYER NAME", yellowCol );
	SCR_DrawVirtualString( winX + winW - 110.0f, startY, 4.8f, "^5TICKS HELD", yellowCol );

	vec4_t divColor = { 1.00f, 0.45f, 0.10f, 0.35f };
	SCR_FillRect( winX + 10.0f, startY + 14.0f, winW - 20.0f, 1.0f, divColor );

	if ( g_rpgTopPotato.count == 0 ) {
		SCR_DrawVirtualString( winX + winW * 0.5f - 60.0f, startY + 40.0f, 5.2f, "^7No potato data.", whiteColor );
	} else {
		float rowY = startY + 20.0f;
		for ( int i = 0; i < g_rpgTopPotato.count && i < 10; i++ ) {
			topPotatoEntry_t *e = &g_rpgTopPotato.entries[i];

			SCR_DrawVirtualString( winX + 16.0f, rowY, 4.8f, va( "^3%d", e->rank ), whiteColor );
			SCR_DrawVirtualString( winX + 45.0f, rowY, 4.8f, va( "^7%s", e->name ), whiteColor );
			SCR_DrawVirtualString( winX + winW - 110.0f, rowY, 4.8f, va( "^1%d ticks", e->ticks ), whiteColor );

			rowY += 17.0f;
		}
	}

	SCR_DrawVirtualString( winX + winW * 0.5f - 80.0f, winY + winH - 12.0f, 4.5f, "^7Press ^1[ESC]^7 to close", whiteColor );
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

	float winW = 460.0f;
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
	}


	vec4_t headerBg = { 0.30f, 0.10f, 0.40f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 24.0f, 3.0f, headerBg, NULL );

	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	SCR_DrawVirtualString( winX + 16.0f, winY + 7.0f, 6.0f, va( "^6ADVENTURE: ^3%s", g_rpgAdv.title[0] ? g_rpgAdv.title : "Quest" ), yellowCol );
	SCR_DrawVirtualString( winX + winW - 40.0f, winY + 7.0f, 5.0f, "^1[ESC]", yellowCol );

	// Story Text
	float startY = winY + 36.0f;
	SCR_DrawVirtualString( winX + 16.0f, startY, 4.8f, va( "^7%s", g_rpgAdv.text ), whiteColor );

	// Virtual Mouse Position
	float mx = (float)g_rpgMouseX;
	float my = (float)g_rpgMouseY;


	// Choices
	float choiceY = winY + 110.0f;
	if ( g_rpgAdv.choice1[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 2.0f && my <= choiceY + 16.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 2.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^5[1] %s", g_rpgAdv.choice1 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^3[1] %s", g_rpgAdv.choice1 ), whiteColor );
		}
		choiceY += 18.0f;
	}
	if ( g_rpgAdv.choice2[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 2.0f && my <= choiceY + 16.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 2.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^5[2] %s", g_rpgAdv.choice2 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^3[2] %s", g_rpgAdv.choice2 ), whiteColor );
		}
		choiceY += 18.0f;
	}
	if ( g_rpgAdv.choice3[0] ) {
		qboolean hover = (qboolean)( mx >= winX + 16.0f && mx <= winX + winW - 16.0f && my >= choiceY - 2.0f && my <= choiceY + 16.0f );
		if ( hover ) {
			vec4_t hCol = { 0.20f, 0.70f, 1.00f, 0.35f };
			SCR_FillRect( winX + 16.0f, choiceY - 2.0f, winW - 32.0f, 16.0f, hCol );
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^5[3] %s", g_rpgAdv.choice3 ), whiteColor );
		} else {
			SCR_DrawVirtualString( winX + 24.0f, choiceY, 4.8f, va( "^3[3] %s", g_rpgAdv.choice3 ), whiteColor );
		}
		choiceY += 18.0f;
	}

	SCR_DrawVirtualString( winX + 20.0f, winY + winH - 12.0f, 4.5f, "^7Click or press ^31^7, ^32^7, ^33^7 to choose  |  ^1!adv^7 or ^1[ESC]^7 to close", whiteColor );

	// Render Visible Mouse Cursor
	static qhandle_t s_hMouseCursor = 0;
	if ( s_hMouseCursor <= 0 && re && re->RegisterShader ) {
		s_hMouseCursor = re->RegisterShader( "ui/assets/selectcursor.tga" );
		if ( s_hMouseCursor <= 0 ) s_hMouseCursor = re->RegisterShader( "ui/assets/sizecursor.tga" );
		if ( s_hMouseCursor <= 0 ) s_hMouseCursor = re->RegisterShader( "ui/assets/cursor.tga" );
		if ( s_hMouseCursor <= 0 ) s_hMouseCursor = re->RegisterShader( "gfx/2d/cursor" );
		if ( s_hMouseCursor <= 0 ) s_hMouseCursor = re->RegisterShader( "gfx/rpg_hud/potato" );
	}
	if ( s_hMouseCursor > 0 ) {
		SCR_DrawPic( mx, my, 24.0f, 24.0f, s_hMouseCursor );
	}

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
SCR_DrawPartyOverlay

WoW-Style RPG Party HUD Overlay
Customizable position via cg_partyX and cg_partyY cvars
==================
*/void SCR_DrawPartyOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	// Real-time automatic BP syncing to server
	static int s_lastSentBP = -1;
	int currentBP = cl.snap.ps.stats[STAT_ARMOR];
	if ( currentBP != s_lastSentBP ) {
		s_lastSentBP = currentBP;
		CL_AddReliableCommand( va( "my_bp %d", currentBP ), qfalse );
	}


	if ( !g_rpgParty.active ) return;

	static cvar_t *cg_partyX = NULL;
	static cvar_t *cg_partyY = NULL;
	if ( !cg_partyX ) cg_partyX = Cvar_Get( "cg_partyX", "15", CVAR_ARCHIVE );
	if ( !cg_partyY ) cg_partyY = Cvar_Get( "cg_partyY", "120", CVAR_ARCHIVE );

	float startX = cg_partyX->value;
	float startY = cg_partyY->value;

	int members = g_rpgParty.memberCount;
	if ( members <= 0 ) return;
	if ( members > MAX_PARTY_MEMBERS ) members = MAX_PARTY_MEMBERS;
	float cardW = 90.0f;
	float cardH = 16.0f + ( members * 20.0f );

	vec4_t bgCol = { 0.05f, 0.08f, 0.14f, 0.85f };
	vec4_t borderCol = { 0.20f, 0.65f, 1.00f, 0.80f };
	vec4_t whiteCol = { 1.00f, 1.00f, 1.00f, 1.00f };
	vec4_t hpBgCol = { 0.15f, 0.05f, 0.05f, 0.70f };
	vec4_t hpFillCol = { 0.15f, 0.85f, 0.25f, 0.90f };
	vec4_t bpBgCol = { 0.05f, 0.15f, 0.25f, 0.70f };
	vec4_t bpFillCol = { 0.15f, 0.65f, 1.00f, 0.90f };

	SCR_FillRect( startX, startY, cardW, cardH, bgCol );
	SCR_FillRect( startX, startY, cardW, 1.0f, borderCol );
	SCR_FillRect( startX, startY + cardH - 1.0f, cardW, 1.0f, borderCol );
	SCR_FillRect( startX, startY, 1.0f, cardH, borderCol );
	SCR_FillRect( startX + cardW - 1.0f, startY, 1.0f, cardH, borderCol );

	// Header: Party Name & Score
	char headerStr[64];
	Com_sprintf( headerStr, sizeof( headerStr ), "^5%.8s ^7[^3%dP^7]", g_rpgParty.teamName[0] ? g_rpgParty.teamName : "Party", g_rpgParty.score );
	SCR_DrawVirtualString( startX + 3.0f, startY + 2.0f, 3.2f, headerStr, whiteCol );

	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.40f };
	SCR_FillRect( startX + 3.0f, startY + 12.0f, cardW - 6.0f, 1.0f, divColor );

	// Render Party Members
	float rowY = startY + 14.0f;
	for ( int i = 0; i < members; i++ ) {
		rpgPartyMember_t *m = &g_rpgParty.members[i];

		// Real-time local player Health & BP update
		if ( m->clientNum == cl.snap.ps.clientNum ) {
			m->health = cl.snap.ps.stats[STAT_HEALTH];
			m->maxHealth = cl.snap.ps.stats[STAT_MAX_HEALTH] > 0 ? cl.snap.ps.stats[STAT_MAX_HEALTH] : 100;
			m->bp = cl.snap.ps.stats[STAT_ARMOR];
		}

		char nameBuf[64];
		Q_strncpyz( nameBuf, m->name[0] ? m->name : "Player", sizeof( nameBuf ) );
		int nameLen = SCR_Strlen( nameBuf );
		float fontScale = 3.6f;
		if ( nameLen > 8 ) fontScale = 3.0f;
		if ( nameLen > 14 ) fontScale = 2.5f;

		char lineStr[96];
		Com_sprintf( lineStr, sizeof( lineStr ), "^3L%d ^7%s", m->level, nameBuf );
		SCR_DrawVirtualString( startX + 3.0f, rowY, fontScale, lineStr, whiteCol );

		// HP Bar (Green) — full width across card
		float barX = startX + 3.0f;
		float barY = rowY + 9.0f;
		float barW = cardW - 6.0f;
		float barH = 2.5f;

		float hpFrac = ( m->maxHealth > 0 ) ? ( (float)m->health / (float)m->maxHealth ) : 1.0f;
		if ( hpFrac < 0.0f ) hpFrac = 0.0f;
		if ( hpFrac > 1.0f ) hpFrac = 1.0f;

		SCR_FillRect( barX, barY, barW, barH, hpBgCol );
		if ( hpFrac > 0.0f ) {
			SCR_FillRect( barX, barY, barW * hpFrac, barH, hpFillCol );
		}

		// BP Bar (Blue) — full width across card
		float bpFrac = ( m->maxBP > 0 ) ? ( (float)m->bp / (float)m->maxBP ) : 1.0f;
		if ( bpFrac < 0.0f ) bpFrac = 0.0f;
		if ( bpFrac > 1.0f ) bpFrac = 1.0f;

		float barY2 = barY + 3.5f;
		SCR_FillRect( barX, barY2, barW, 2.0f, bpBgCol );
		if ( bpFrac > 0.0f ) {
			SCR_FillRect( barX, barY2, barW * bpFrac, 2.0f, bpFillCol );
		}

		rowY += 20.0f;
	}
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
		s_hBarBg = 0;
		s_hBarFill = 0;
		s_hAvatar = 0;
		s_hAvatarFrame = 0;
		s_hModalBg = 0;
		s_hWantedBg = 0;
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
					SCR_DrawRPGHUDOverlay();
					SCR_DrawDemoRecording();
					SCR_DrawLeaderboardOverlay();
					SCR_DrawStatsOverlay();
					SCR_DrawToastOverlay();
					SCR_DrawInspectOverlay();
					SCR_DrawBountyOverlay();
					SCR_DrawShopOverlay();
					SCR_DrawQuestInvOverlay();
					SCR_DrawAchievementsOverlay();
					SCR_DrawTopCreditsOverlay();
					SCR_DrawTopPotatoOverlay();
					SCR_DrawAdventureOverlay();
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
