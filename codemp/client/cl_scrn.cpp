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
#include "cl_xp_profile.h"
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
cvar_t		*cg_rpg_hud_style;
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
cvar_t		*cg_rpg_toast_pos;
cvar_t		*cg_rpg_notif_pos;
cvar_t		*cg_rpg_duel_popups;
qboolean	g_rpgResetConfirm = qfalse;
rpgPlayerStats_t g_rpgStats;
rpgToastNotif_t  g_rpgToast   = {qfalse, qfalse, 0, 0, 0, "", 0, 0, 0, 0};
rpgInspectCard_t g_rpgInspect = {qfalse, 1, 1000, "Padawan", "", 0};
rpgBountyOverlay_t g_rpgBounty = {qfalse, qfalse, 0, {}};

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
	cg_rpg_hud_style = Cvar_Get ("cg_rpg_hud_style", "0", CVAR_ARCHIVE);
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
	cg_rpg_toast_pos = Cvar_Get ("cg_rpg_toast_pos", "1", CVAR_ARCHIVE);
	cg_rpg_notif_pos = Cvar_Get ("cg_rpg_notif_pos", "1", CVAR_ARCHIVE);
	cg_rpg_duel_popups = Cvar_Get ("cg_rpg_duel_popups", "0", CVAR_ARCHIVE);

	Cmd_AddCommand( "rpg_hud_style", SCR_RPGHUDStyle_f, "Select RPG HUD style: classic (0) or bottom (1)" );
	Cmd_AddCommand( "rpg_hud_pos", SCR_RPGHUDPos_f, "Position RPG HUD: left, right, bottomright, bottomleft, bottomcenter" );

	CL_XP_Init();

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
void SCR_DrawVirtualString( float x, float y, float charSize, const char *string, const float *setColor ) {
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
static void SCR_DrawJediVectorEmblem( float cx, float cy, float radius, int faction ) {
	if ( faction == FACTION_SITH ) {
		// --- SITH EMPIRE VECTOR EMBLEM (Crimson/Red) ---
		vec4_t sithRed   = { 0.95f, 0.15f, 0.15f, 0.85f };
		vec4_t saberCore = { 1.00f, 0.90f, 0.90f, 0.95f };

		// Sith Star Destroyer Hex Crest
		SCR_FillRect( cx - 1.0f, cy - radius + 4.0f, 2.0f, radius * 2.0f - 8.0f, sithRed );
		SCR_FillRect( cx - radius + 4.0f, cy - 1.0f, radius * 2.0f - 8.0f, 2.0f, sithRed );
		SCR_FillRect( cx - 1.0f, cy - 6.0f, 2.0f, 12.0f, saberCore );
		SCR_FillRect( cx - 6.0f, cy - 1.0f, 12.0f, 2.0f, saberCore );
	} else {
		// --- JEDI ORDER VECTOR EMBLEM (Cyan/Gold) ---
		vec4_t saberGold = { 1.00f, 0.85f, 0.20f, 0.95f };
		vec4_t saberCyan = { 0.20f, 0.90f, 1.00f, 0.95f };

		// Jedi Lightsaber Wings & Central Blade
		SCR_FillRect( cx - 1.0f, cy - radius + 4.0f, 2.0f, radius * 2.0f - 8.0f, saberCyan ); // Vertical blade
		SCR_FillRect( cx - 5.0f, cy, 10.0f, 1.5f, saberGold );                                // Crossguard
		SCR_FillRect( cx - 7.0f, cy - 3.0f, 3.0f, 1.5f, saberGold );                          // Left wing tip
		SCR_FillRect( cx + 4.0f, cy - 3.0f, 3.0f, 1.5f, saberGold );                          // Right wing tip
		SCR_FillRect( cx - 3.0f, cy + 3.0f, 6.0f, 1.5f, saberGold );                          // Hilt base
	}
}

static void SCR_DrawRPGAvatar( float x, float y, float size, int faction ) {
	int avatarIdx = cg_rpg_avatar ? cg_rpg_avatar->integer : 0;
	if ( avatarIdx == 0 ) {
		SCR_DrawJediVectorEmblem( x + size * 0.5f, y + size * 0.5f, size * 0.5f, faction );
		return;
	}

	const char *avatarShaderName = "gfx/2d/logos/mb_jediorder";
	if ( avatarIdx == 1 ) avatarShaderName = "gfx/2d/logos/mb_jediorder";
	else if ( avatarIdx == 2 ) avatarShaderName = "gfx/2d/logos/mb_sithempire";
	else if ( avatarIdx == 3 ) avatarShaderName = "gfx/2d/logos/mb_mand";
	else if ( avatarIdx == 4 ) avatarShaderName = "gfx/2d/logos/mb_rebel";
	else if ( avatarIdx == 5 ) avatarShaderName = "gfx/2d/logos/mb_empire";
	else if ( avatarIdx == 6 ) avatarShaderName = "gfx/2d/logos/mb_bountyhunters";
	else if ( avatarIdx == 7 ) avatarShaderName = "gfx/2d/logos/mb_oldrepublic";
	else if ( avatarIdx >= 8 && avatarIdx <= 15 ) {
		static char customPath[64];
		Com_sprintf( customPath, sizeof(customPath), "gfx/rpg_hud/avatars/avatar_custom%d", avatarIdx - 7 );
		avatarShaderName = customPath;
	}

	if ( re && re->RegisterShader ) {
		qhandle_t hShader = re->RegisterShader( avatarShaderName );
		if ( hShader > 0 ) {
			SCR_DrawPic( x, y, size, size, hShader );
			return;
		}
	}

	// Fallback to vector emblem if shader not found
	SCR_DrawJediVectorEmblem( x + size * 0.5f, y + size * 0.5f, size * 0.5f, faction );
}

/*
==================
SCR_DrawRPGHUDOverlay

Renders client-side RPG HUD Overlay supporting 5 Star Wars HUD styles (cg_rpg_hud_style 0 to 4)
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
static qhandle_t s_hHoloJediFrame       = 0;
static qhandle_t s_hHoloSithFrame       = 0;
static qhandle_t s_hFrameStyle1Saber    = 0;
static qhandle_t s_hFrameStyle2Pill     = 0;
static qhandle_t s_hFrameStyle3Imperial = 0;
static qhandle_t s_hFrameStyle4Neon     = 0;
static qhandle_t s_hWinSettings         = 0;
static qhandle_t s_hWinRanks            = 0;
static qhandle_t s_hWinStats            = 0;
static qhandle_t s_hWinHelp             = 0;
static qhandle_t s_hBtnNormal           = 0;
static qhandle_t s_hBtnHover            = 0;
static qhandle_t s_hBtnActive           = 0;
static qhandle_t s_hBtnSithNormal       = 0;
static qhandle_t s_hBtnSithHover        = 0;
static qhandle_t s_hMousePointer        = 0;
static qhandle_t s_hSaberJediBar        = 0;
static qhandle_t s_hSaberSithBar        = 0;
static qhandle_t s_hFillXPJedi         = 0;
static qhandle_t s_hFillXPSith         = 0;
static qboolean  s_shadersTried         = qfalse;

static int SCR_GetCleanStringLength( const char *str ) {
	if ( !str ) return 0;
	int len = 0;
	for ( const char *p = str; *p; p++ ) {
		if ( *p == '^' && *(p + 1) != '\0' && *(p + 1) != '^' ) {
			p++; // skip color escape code char
		} else {
			len++;
		}
	}
	return len;
}

static float SCR_GetTextPixelWidth( const char *str, float charScale ) {
	int cleanLen = SCR_GetCleanStringLength( str );
	return (float)cleanLen * charScale * 0.60f;
}

void SCR_TruncateTextToWidth( const char *inText, float maxWidth, float charScale, char *outBuf, int outBufSize ) {
	if ( !inText || !outBuf || outBufSize <= 0 ) return;
	outBuf[0] = '\0';
	float curWidth = SCR_GetTextPixelWidth( inText, charScale );
	if ( curWidth <= maxWidth ) {
		Q_strncpyz( outBuf, inText, outBufSize );
		return;
	}

	int inLen = strlen( inText );
	if ( inLen > outBufSize - 4 ) inLen = outBufSize - 4;
	for ( int i = inLen; i > 0; i-- ) {
		char temp[256];
		Q_strncpyz( temp, inText, i + 1 );
		Q_strcat( temp, sizeof(temp), ".." );
		if ( SCR_GetTextPixelWidth( temp, charScale ) <= maxWidth ) {
			Q_strncpyz( outBuf, temp, outBufSize );
			return;
		}
	}
	Q_strncpyz( outBuf, "..", outBufSize );
}

void SCR_DrawCenteredText( float boxX, float boxY, float boxW, float charScale, const char *str, vec4_t color ) {
	float textWidth = SCR_GetTextPixelWidth( str, charScale );
	float centeredX = boxX + (boxW - textWidth) * 0.5f;
	if ( centeredX < boxX + 2.0f ) centeredX = boxX + 2.0f;
	SCR_DrawVirtualString( centeredX, boxY, charScale, str, color );
}

void SCR_DrawRightAlignedText( float rightX, float boxY, float charScale, const char *str, vec4_t color ) {
	float textWidth = SCR_GetTextPixelWidth( str, charScale );
	float alignedX = rightX - textWidth;
	SCR_DrawVirtualString( alignedX, boxY, charScale, str, color );
}

static void SCR_DrawPulsatingXPEnergyBar( float barX, float barY, float barW, float barH, float xpRatio, int faction ) {
	float fillW = barW * xpRatio;
	if ( fillW < 2.0f ) fillW = 2.0f;

	float pulse = 0.75f + 0.25f * sinf( (float)cls.realtime * 0.006f );

	if ( faction == FACTION_SITH ) {
		// --- SITH CRIMSON PULSATING ENERGY BAR ---
		vec4_t sithFill   = { 0.90f, 0.10f, 0.10f, 0.90f * pulse };
		vec4_t sithCore   = { 1.00f, 0.85f, 0.30f, 0.95f };

		SCR_FillRect( barX, barY, fillW, barH, sithFill );
		if ( fillW > 6.0f ) {
			SCR_FillRect( barX + 1.0f, barY + barH * 0.35f, fillW - 2.0f, 1.2f, sithCore );
		}
	} else {
		// --- JEDI CYAN/BLUE PULSATING ENERGY BAR ---
		vec4_t jediFill   = { 0.00f, 0.60f, 0.95f, 0.90f * pulse };
		vec4_t jediCore   = { 0.90f, 0.98f, 1.00f, 0.95f };

		SCR_FillRect( barX, barY, fillW, barH, jediFill );
		if ( fillW > 6.0f ) {
			SCR_FillRect( barX + 1.0f, barY + barH * 0.35f, fillW - 2.0f, 1.2f, jediCore );
		}
	}
}

void SCR_DrawRPGHUDOverlay( void ) {
	static int s_lastState = -1;
	if ( cls.state != s_lastState ) {
		s_lastState = cls.state;
		s_shadersTried = qfalse;
	}

	if ( cls.state != CA_ACTIVE ) {
		return;
	}

	CL_XP_CheckGameEvents();

	if ( g_xpDrawCard ) {
		SCR_DrawProfileCardOverlay();
	}
	if ( g_xpDrawRanks ) {
		SCR_DrawRanksWindowOverlay();
	}
	if ( g_xpDrawHelp ) {
		SCR_DrawHelpWindowOverlay();
	}
	if ( g_xpDrawSettings ) {
		SCR_DrawSettingsWindowOverlay();
	}

	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Hide RPG HUD when Scoreboard (TAB), UI Menus, CGame Menus, or Console are active
	if ( Key_GetCatcher() & (KEYCATCH_UI | KEYCATCH_CGAME | KEYCATCH_CONSOLE) ) {
		return;
	}
	if ( cl.snap.ps.pm_flags & PMF_SCOREBOARD ) {
		return;
	}
	if ( cl.snap.ps.pm_type == PM_INTERMISSION ) {
		return;
	}
	if ( Key_IsDown( A_TAB ) || Key_IsDown( Key_StringToKeynum("tab") ) ) {
		return;
	}

	// Register HD TGA Shaders dynamically upon map load/reconnect
	static int s_lastServerTime = -1;
	static int s_lastClState = -1;
	if ( !s_shadersTried || cls.state != s_lastClState || cl.snap.serverTime < s_lastServerTime ) {
		s_shadersTried = qtrue;
		s_lastClState = cls.state;
		s_lastServerTime = cl.snap.serverTime;
		if ( re && re->RegisterShaderNoMip ) {
			s_hHoloJediFrame       = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style0_clean" );
			s_hHoloSithFrame       = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style0_sith_clean" );
			s_hFrameStyle1Saber    = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style1_clean" );
			s_hFrameStyle2Pill     = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style2_clean" );
			s_hFrameStyle3Imperial = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style3_clean" );
			s_hFrameStyle4Neon     = re->RegisterShaderNoMip( "gfx/rpg_hud/hud_style3_sith_clean" );
			s_hBtnNormal           = re->RegisterShaderNoMip( "gfx/rpg_hud/btn_normal" );
			s_hBtnHover            = re->RegisterShaderNoMip( "gfx/rpg_hud/btn_hover" );
			s_hBtnActive           = re->RegisterShaderNoMip( "gfx/rpg_hud/btn_active" );
			s_hBtnSithNormal       = re->RegisterShaderNoMip( "gfx/rpg_hud/btn_sith_normal" );
			s_hBtnSithHover        = re->RegisterShaderNoMip( "gfx/rpg_hud/btn_sith_hover" );
			s_hMousePointer        = re->RegisterShaderNoMip( "gfx/rpg_hud/mouse_pointer" );
			s_hFillXPJedi          = re->RegisterShaderNoMip( "gfx/rpg_hud/bar_fill_xp_jedi" );
			s_hFillXPSith          = re->RegisterShaderNoMip( "gfx/rpg_hud/bar_fill_xp_sith" );
		}
	}

	int style = 0;
	if ( cg_rpg_hud_style ) {
		style = cg_rpg_hud_style->integer;
	} else if ( cg_rpg_style ) {
		style = cg_rpg_style->integer;
	}
	if ( style < 0 || style > 4 ) style = 0;

	cvar_t *clName = Cvar_Get( "name", "Padawan", 0 );
	const char *rawPlayerName = (cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : (clName ? clName->string : "Player");
	const char *rawRankTitle = (cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : CL_XP_GetRankTitle(g_xpProfile.level, g_xpProfile.faction);

	// Truncate player name & rank title to prevent UI text overflow
	char playerName[64], rankTitle[64];
	SCR_TruncateTextToWidth( rawPlayerName, 95.0f, 5.2f, playerName, sizeof(playerName) );
	SCR_TruncateTextToWidth( rawRankTitle, 45.0f, 3.6f, rankTitle, sizeof(rankTitle) );

	// Fixed invariant panel size (192x48)
	float panelW = 192.0f;
	float panelH = 48.0f;
	float defaultX = 14.0f;
	float defaultY = 345.0f;

	if ( cg_rpg_hud_pos ) {
		int posVal = cg_rpg_hud_pos->integer;
		if ( posVal == 0 ) { defaultX = 14.0f; defaultY = 14.0f; }
		else if ( posVal == 1 ) { defaultX = 640.0f - panelW - 14.0f; defaultY = 14.0f; }
		else if ( posVal == 2 ) { defaultX = 14.0f; defaultY = 345.0f; } // Above health/armor UI
		else if ( posVal == 3 ) { defaultX = 640.0f - panelW - 14.0f; defaultY = 345.0f; } // Above force/weapon UI
		else if ( posVal == 4 ) { defaultX = 320.0f - panelW * 0.5f; defaultY = 425.0f; }
	}

	float customX = cg_rpg_x ? cg_rpg_x->value : 0.0f;
	float customY = cg_rpg_y ? cg_rpg_y->value : 0.0f;
	float panelX = (customX != 0.0f) ? customX : defaultX;
	float panelY = (customY != 0.0f) ? customY : defaultY;

	int level = CL_XP_GetLevel();
	int curXP = 0, xpMax = 0;
	float levelProgressPercent = 0.0f;
	CL_XP_GetLevelProgress( &curXP, &xpMax, &levelProgressPercent );

	static float s_visualXP = -1.0f;
	if ( s_visualXP < 0.0f ) {
		s_visualXP = (float)curXP;
	} else {
		float diff = (float)curXP - s_visualXP;
		if ( fabsf(diff) > 0.5f ) {
			s_visualXP += diff * 0.10f;
		} else {
			s_visualXP = (float)curXP;
		}
	}

	float xpRatio = s_visualXP / (float)(xpMax > 0 ? xpMax : 1000);
	if ( xpRatio < 0.0f ) xpRatio = 0.0f;
	if ( xpRatio > 1.0f ) xpRatio = 1.0f;

	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	re->SetColor( whiteColor );

	// Faction colors & pulse animations (smooth sine wave glow)
	float glowPulse = sinf( (float)cls.realtime * 0.0035f );
	float pulseAlpha = 0.70f + 0.25f * glowPulse;

	vec4_t factionPrimary = { 0.00f, 0.75f, 1.00f, pulseAlpha }; // Cyan/Blue for Jedi
	if ( g_xpProfile.faction == FACTION_SITH ) {
		factionPrimary[0] = 0.95f; factionPrimary[1] = 0.15f; factionPrimary[2] = 0.15f; // Crimson Red
	}

	// ========================================================
	// STYLE 0: MODERN STAR WARS HOLOGRAPHIC DATAPAD (Default)
	// ========================================================
if ( style == 0 ) {
	qhandle_t hFrame = (g_xpProfile.faction == FACTION_SITH) ? s_hHoloSithFrame : s_hHoloJediFrame;
	if ( hFrame > 0 ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, hFrame );
	} else {
		vec4_t glassBg     = { 0.02f, 0.06f, 0.14f, 0.75f };
		vec4_t glassBorder = { factionPrimary[0], factionPrimary[1], factionPrimary[2], pulseAlpha };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, glassBg, glassBorder );
	}

	// Centered inside circle avatar ring frame
	float avatarX = panelX + 12.0f;
	float avatarY = panelY + 6.0f;
	float avatarSize = 24.0f;
	SCR_DrawRPGAvatar( avatarX, avatarY, avatarSize, g_xpProfile.faction );

	// Level Badge inside mini ring at bottom of circle
	char levelStr[32];
	Com_sprintf( levelStr, sizeof(levelStr), "^3Lv%d", level );
	SCR_DrawCenteredText( panelX + 8.0f, panelY + 36.0f, 32.0f, 3.8f, levelStr, whiteColor );

	// Player Info Column
	float textX = panelX + 46.0f;
	SCR_DrawVirtualString( textX, panelY + 6.5f, 5.2f, va("^7%s", playerName), whiteColor );
	SCR_DrawVirtualString( textX, panelY + 18.0f, 4.0f, va("^3%s ^7|^2%dK^7/^1%dD ^7|^2%dW^7/^1%dL", rankTitle, g_xpProfile.kills, g_xpProfile.deaths, g_xpProfile.duelWins, g_xpProfile.duelLosses), whiteColor );

	// Hardcoded Pulsating Energy XP Progress Bar Track
	float barX = textX + 2.0f;
	float barY = panelY + 31.5f;
	float barW = panelX + panelW - barX - 10.0f;
	float barH = 7.0f;

	SCR_DrawPulsatingXPEnergyBar( barX, barY, barW, barH, xpRatio, g_xpProfile.faction );

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^7%d / %d XP", (int)s_visualXP, xpMax );
	SCR_DrawCenteredText( barX, barY - 0.5f, barW, 3.5f, xpText, whiteColor );
}
// ========================================================
// STYLE 1: SLANTED PARALLELOGRAM & DIAMOND AVATAR
// ========================================================
else if ( style == 1 ) {
	if ( s_hFrameStyle1Saber > 0 ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, s_hFrameStyle1Saber );
	} else {
		vec4_t hiltBg     = { 0.03f, 0.05f, 0.10f, 0.85f };
		vec4_t hiltBorder = { factionPrimary[0], factionPrimary[1], factionPrimary[2], pulseAlpha };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, hiltBg, hiltBorder );
	}

	float avatarX = panelX + 12.0f;
	float avatarY = panelY + 12.0f;
	float avatarSize = 24.0f;
	SCR_DrawRPGAvatar( avatarX, avatarY, avatarSize, g_xpProfile.faction );

	float textX = panelX + 48.0f;
	SCR_DrawVirtualString( textX, panelY + 6.5f, 5.2f, va("^7%s ^3Lv%d", playerName, level), whiteColor );
	SCR_DrawVirtualString( textX, panelY + 18.0f, 4.0f, va("^3%s ^7|^2%dK^7/^1%dD ^7|^2%dW^7/^1%dL", rankTitle, g_xpProfile.kills, g_xpProfile.deaths, g_xpProfile.duelWins, g_xpProfile.duelLosses), whiteColor );

	// Hardcoded Pulsating Energy XP Progress Bar Track
	float barX = textX + 2.0f;
	float barY = panelY + 31.5f;
	float barW = panelX + panelW - barX - 14.0f;
	float barH = 7.0f;

	SCR_DrawPulsatingXPEnergyBar( barX, barY, barW, barH, xpRatio, g_xpProfile.faction );

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^7%d / %d XP", (int)s_visualXP, xpMax );
	SCR_DrawCenteredText( barX, barY - 0.5f, barW, 3.5f, xpText, whiteColor );
}
// ========================================================
// STYLE 2: CURVED PILOT ARC & OVAL AVATAR
// ========================================================
else if ( style == 2 ) {
	if ( s_hFrameStyle2Pill > 0 ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, s_hFrameStyle2Pill );
	} else {
		vec4_t pillBg = { 0.03f, 0.07f, 0.14f, 0.80f };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, pillBg, factionPrimary );
	}

	float avatarX = panelX + 12.0f;
	float avatarY = panelY + 12.0f;
	float avatarSize = 24.0f;
	SCR_DrawRPGAvatar( avatarX, avatarY, avatarSize, g_xpProfile.faction );

	float textX = panelX + 46.0f;
	SCR_DrawVirtualString( textX, panelY + 6.5f, 5.2f, va("^7%s ^3Lv%d", playerName, level), whiteColor );
	SCR_DrawVirtualString( textX, panelY + 18.0f, 4.0f, va("^3%s ^7|^2%dK^7/^1%dD ^7|^2%dW^7/^1%dL", rankTitle, g_xpProfile.kills, g_xpProfile.deaths, g_xpProfile.duelWins, g_xpProfile.duelLosses), whiteColor );

	float barX = textX + 2.0f;
	float barY = panelY + 31.5f;
	float barW = panelX + panelW - barX - 12.0f;
	float barH = 7.0f;

	SCR_DrawPulsatingXPEnergyBar( barX, barY, barW, barH, xpRatio, g_xpProfile.faction );

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^7%d / %d XP", (int)s_visualXP, xpMax );
	SCR_DrawCenteredText( barX, barY - 0.5f, barW, 3.5f, xpText, whiteColor );
}
// ========================================================
// STYLE 3 / 4: CHAMFERED DATAPAD & HEXAGON AVATAR
// ========================================================
else if ( style == 3 || style == 4 ) {
	qhandle_t hImpFrame = (g_xpProfile.faction == FACTION_SITH && s_hFrameStyle4Neon > 0) ? s_hFrameStyle4Neon : s_hFrameStyle3Imperial;
	if ( hImpFrame > 0 ) {
		SCR_DrawPic( panelX, panelY, panelW, panelH, hImpFrame );
	} else {
		vec4_t impBg     = { 0.07f, 0.08f, 0.10f, 0.90f };
		vec4_t impBorder = { factionPrimary[0], factionPrimary[1], factionPrimary[2], 0.85f };
		SCR_DrawMBIICapsule( panelX, panelY, panelW, panelH, impBg, impBorder );
	}

	float avatarX = panelX + 12.0f;
	float avatarY = panelY + 12.0f;
	float avatarSize = 24.0f;
	SCR_DrawRPGAvatar( avatarX, avatarY, avatarSize, g_xpProfile.faction );

	float textX = panelX + 46.0f;
	SCR_DrawVirtualString( textX, panelY + 6.5f, 5.2f, va("^7%s ^3Lv%d", playerName, level), whiteColor );
	SCR_DrawVirtualString( textX, panelY + 18.0f, 4.0f, va("^3%s ^7|^2%dK^7/^1%dD ^7|^2%dW^7/^1%dL", rankTitle, g_xpProfile.kills, g_xpProfile.deaths, g_xpProfile.duelWins, g_xpProfile.duelLosses), whiteColor );

	float barX = textX + 2.0f;
	float barY = panelY + 31.5f;
	float barW = panelX + panelW - barX - 12.0f;
	float barH = 7.0f;

	SCR_DrawPulsatingXPEnergyBar( barX, barY, barW, barH, xpRatio, g_xpProfile.faction );

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^7%d / %d XP", (int)s_visualXP, xpMax );
	SCR_DrawCenteredText( barX, barY - 0.5f, barW, 3.5f, xpText, whiteColor );
}

	// Render HUD popup notifications at top-center (animated cards)
	CL_XP_DrawNotifications();

	if ( g_xpDrawCard ) {
		SCR_DrawProfileCardOverlay();
	}
}

/*
==================
SCR_DrawRPGNotificationCard

Renders single animated HUD popup notification at top-center of screen.
Slide-in from top during first 200ms, then last 500ms fade-out.
Card width scales with text length; tint color matches notification type.
==================
*/
void SCR_DrawRPGNotificationCard( const rpgNotif_t *notif, int posIndex, int visibleCount ) {
	if ( !notif ) return;

	int now = cls.realtime;
	int age = now - notif->startMs;
	if ( age < 0 ) age = 0;

	// Compute alpha with fade-in (0-200ms) and fade-out (last 500ms) ramp
	float totalLife = (float)( notif->lifetimeMs > 0 ? notif->lifetimeMs : NOTIF_LIFETIME_MS );
	float fadeInEnd = 200.0f;
	float fadeOutStart = totalLife - 500.0f;
	float alpha = 1.0f;
	if ( age < fadeInEnd ) {
		alpha = (float)age / fadeInEnd;
	} else if ( (float)age > fadeOutStart && totalLife > 600.0f ) {
		float tail = totalLife - (float)age;
		if ( tail < 0.0f ) tail = 0.0f;
		alpha = tail / 500.0f;
	}
	if ( alpha < 0.0f ) alpha = 0.0f;
	if ( alpha > 1.0f ) alpha = 1.0f;

	// Slide-in offset: first 200ms slide down from -24px offset above target
	float slidePx = 0.0f;
	if ( age < 200 ) {
		slidePx = -24.0f * ( 1.0f - ( (float)age / 200.0f ) );
	}

	// Text length based sizing
	int titleClean = SCR_GetCleanStringLength( notif->text );
	int subClean   = SCR_GetCleanStringLength( notif->subtext );
	int maxChars   = ( titleClean > subClean + 8 ) ? titleClean : ( subClean + 8 );
	if ( notif->xpDelta != 0 ) maxChars += 10;

	float minW  = 160.0f;
	float cardW = 16.0f + (float)maxChars * 6.0f;
	if ( cardW < minW ) cardW = minW;
	if ( cardW > 400.0f ) cardW = 400.0f;
	float cardH = ( notif->subtext[0] ) ? 46.0f : 32.0f;
	float pad   = 6.0f;

	int curPos = cg_rpg_notif_pos ? cg_rpg_notif_pos->integer : 1;
	float centerX = 320.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 28.0f + ( (float)posIndex * ( cardH + pad ) ) + slidePx;

	if ( curPos == 0 ) { // Top-Left
		centerX = 14.0f + cardW * 0.5f;
		cardX = 14.0f;
		cardY = 80.0f + ( (float)posIndex * ( cardH + pad ) ) + slidePx;
	} else if ( curPos == 1 ) { // Top-Center
		centerX = 320.0f;
		cardX = centerX - cardW * 0.5f;
		cardY = 28.0f + ( (float)posIndex * ( cardH + pad ) ) + slidePx;
	} else if ( curPos == 2 ) { // Top-Right
		centerX = 640.0f - 14.0f - cardW * 0.5f;
		cardX = 640.0f - 14.0f - cardW;
		cardY = 80.0f + ( (float)posIndex * ( cardH + pad ) ) + slidePx;
	} else if ( curPos == 3 ) { // Bot-Left
		centerX = 14.0f + cardW * 0.5f;
		cardX = 14.0f;
		cardY = 320.0f - ( (float)posIndex * ( cardH + pad ) ) - slidePx;
	} else if ( curPos == 4 ) { // Bot-Right
		centerX = 640.0f - 14.0f - cardW * 0.5f;
		cardX = 640.0f - 14.0f - cardW;
		cardY = 320.0f - ( (float)posIndex * ( cardH + pad ) ) - slidePx;
	}

	// Tint border + darker tinted body copy the r, g, b from notif tint
	vec4_t borderColor;
	VectorCopy4( notif->tint, borderColor );
	borderColor[3] = alpha * notif->tint[3];

	vec4_t bgColor;
	bgColor[0] = notif->tint[0] * 0.10f + 0.02f;
	bgColor[1] = notif->tint[1] * 0.10f + 0.04f;
	bgColor[2] = notif->tint[2] * 0.12f + 0.06f;
	bgColor[3] = 0.90f * alpha;

	SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );

	// Main title text (uses tint color, but brighten it a bit)
	vec4_t titleCol;
	titleCol[0] = ( notif->tint[0] * 0.55f ) + 0.45f;
	titleCol[1] = ( notif->tint[1] * 0.55f ) + 0.45f;
	titleCol[2] = ( notif->tint[2] * 0.55f ) + 0.45f;
	titleCol[3] = alpha;

	char titleBuf[160];
	if ( notif->xpDelta > 0 ) {
		Com_sprintf( titleBuf, sizeof( titleBuf ), "^7%s   ^2+%d XP", notif->text, notif->xpDelta );
	} else {
		Q_strncpyz( titleBuf, notif->text, sizeof( titleBuf ) );
	}
	int titlePx = SCR_GetCleanStringLength( titleBuf );
	float titleX = centerX - ( (float)titlePx * 6.2f * 0.60f ) * 0.5f;
	SCR_DrawVirtualString( titleX, cardY + 5.0f, 6.2f, titleBuf, titleCol );

	// Sub-text row
	if ( notif->subtext[0] ) {
		vec4_t subCol = { 0.90f, 0.90f, 0.95f, alpha };
		int subPx = SCR_GetCleanStringLength( notif->subtext );
		float subX = centerX - ( (float)subPx * 4.8f * 0.60f ) * 0.5f;
		SCR_DrawVirtualString( subX, cardY + 23.0f, 4.8f, notif->subtext, subCol );
	}
}

/*
==================
CL_XP_DrawNotifications

Iterates all active notification slots and delegates to the card renderer.
==================
*/
void CL_XP_DrawNotifications( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	if ( !cg_rpg_notify_popups || !cg_rpg_notify_popups->integer ) return;
	if ( g_rpgNotifCount <= 0 ) return;

	SCR_DrawRPGNotificationCard( &g_rpgNotifs[0], 0, 1 );
}

void SCR_DrawProfileCardOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	const char *profName = CL_XP_GetProfileName();
	const char *rankTitle = CL_XP_GetRankTitle( g_xpProfile.level, g_xpProfile.faction );

	float cardW = 560.0f;
	float cardH = 380.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 240.0f - cardH * 0.5f;

	vec4_t whiteColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t bgColor     = { 0.02f, 0.04f, 0.08f, 0.94f };
	vec4_t borderColor = { 0.00f, 0.70f, 0.95f, 0.95f };

	if ( g_xpProfile.faction == FACTION_SITH ) {
		borderColor[0] = 0.95f; borderColor[1] = 0.15f; borderColor[2] = 0.15f;
	}

	if ( s_hWinStats > 0 ) {
		SCR_DrawPic( cardX, cardY, cardW, cardH, s_hWinStats );
	} else {
		SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );
	}

	// Large Avatar Picture at top-left
	float avX = cardX + 24.0f;
	float avY = cardY + 18.0f;
	float avSize = 44.0f;
	SCR_DrawRPGAvatar( avX, avY, avSize, g_xpProfile.faction );

	// Header: Name & Level Badge next to avatar emblem
	char headerStr[128];
	Com_sprintf( headerStr, sizeof(headerStr), "^7%s ^3[Lvl %d %s^3]",
		profName, g_xpProfile.level, (g_xpProfile.faction == FACTION_SITH) ? "^1SITH EMPIRE" : "^6JEDI ORDER" );
	SCR_DrawVirtualString( cardX + 78.0f, cardY + 20.0f, 6.8f, headerStr, whiteColor );

	char rankSub[96];
	Com_sprintf( rankSub, sizeof(rankSub), "^3Rank Title: ^7%s", rankTitle );
	SCR_DrawVirtualString( cardX + 78.0f, cardY + 42.0f, 5.5f, rankSub, whiteColor );

	// --- XP Progress ---
	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress( &curXP, &reqXP, &percent );

	char xpStr[96];
	Com_sprintf( xpStr, sizeof(xpStr), "^7XP Progress: ^3%d ^7/ ^3%d XP ^7(^2%.1f%%^7)", curXP, reqXP, percent * 100.0f );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 75.0f, 5.5f, xpStr, whiteColor );

	// --- Combat Stats ---
	float kdRatio = (g_xpProfile.deaths > 0) ? ((float)g_xpProfile.kills / (float)g_xpProfile.deaths) : (float)g_xpProfile.kills;
	char kdStr[128];
	Com_sprintf( kdStr, sizeof(kdStr), "^7Kills: ^3%d  ^7Deaths: ^1%d  ^7K/D: ^2%.2f  ^7NPC: ^3%d",
		g_xpProfile.kills, g_xpProfile.deaths, kdRatio, g_xpProfile.npcKills );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 110.0f, 5.5f, kdStr, whiteColor );

	char weaponStr[128];
	Com_sprintf( weaponStr, sizeof(weaponStr), "^7Saber Kills: ^3%d   ^7Gunner Kills: ^3%d   ^7Lifetime XP: ^3%d",
		g_xpProfile.saberKills, g_xpProfile.gunnerKills, g_xpProfile.xp );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 140.0f, 5.5f, weaponStr, whiteColor );

	// --- MB2 Game Mode Stats ---
	int totalRounds = g_xpProfile.roundWins + g_xpProfile.roundLosses;
	float roundWinRate = (totalRounds > 0) ? (((float)g_xpProfile.roundWins / (float)totalRounds) * 100.0f) : 0.0f;
	int totalDuels = g_xpProfile.duelWins + g_xpProfile.duelLosses;
	float duelWinRate = (totalDuels > 0) ? (((float)g_xpProfile.duelWins / (float)totalDuels) * 100.0f) : 0.0f;

	char recordStr[192];
	Com_sprintf( recordStr, sizeof(recordStr), "^7Team: ^2%dW^7-^1%dL^7 (^3%.0f%%^7)  ^7Duels: ^2%dW^7-^1%dL^7 (^3%.0f%%^7)",
		g_xpProfile.roundWins, g_xpProfile.roundLosses, roundWinRate,
		g_xpProfile.duelWins, g_xpProfile.duelLosses, duelWinRate );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 175.0f, 5.5f, recordStr, whiteColor );

	// --- Duel Achievements & Streaks ---
	char duelAch[192];
	Com_sprintf( duelAch, sizeof(duelAch),
		"^7Perfect: ^3%d   ^7QuickDraw: ^3%d   ^7Endurance: ^3%d   ^7Streak: ^3%d",
		g_xpProfile.perfectWins, g_xpProfile.quickDrawWins, g_xpProfile.enduranceWins, g_xpProfile.currentDuelStreak );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 210.0f, 5.2f, duelAch, whiteColor );

	// --- Playtime + Multi-kills ---
	float hrs = (float)g_xpProfile.totalPlaytimeMs / (1000.0f * 3600.0f);
	char timeStr[192];
	Com_sprintf( timeStr, sizeof(timeStr), "^7Playtime: ^3%.1f hrs   ^7Multi-Kills: ^32x=%d 3x=%d 4x=%d 5x=%d 6+=%d",
		hrs, g_xpProfile.multiDouble, g_xpProfile.multiTriple, g_xpProfile.multiOverkill,
		g_xpProfile.multiMonster, g_xpProfile.multiUltra );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 245.0f, 5.2f, timeStr, whiteColor );

	// XP grants legend
	char xpLegend[128];
	Com_sprintf( xpLegend, sizeof(xpLegend), "^7XP Grants: ^3Kill +%d  Duel +%d  Round +%d  NPC +%d",
		XP_GRANT_PLAYER_KILL, XP_GRANT_DUEL_WIN, XP_GRANT_ROUND_WIN, XP_GRANT_NPC_KILL );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 280.0f, 4.8f, xpLegend, whiteColor );

	SCR_DrawVirtualString( cardX + 24.0f, cardY + cardH - 22.0f, 5.0f, "^5Press ESC or type !stats to close", whiteColor );
}

void SCR_DrawRanksWindowOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	float cardW = 540.0f;
	float cardH = 340.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 240.0f - cardH * 0.5f;

	vec4_t whiteColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t bgColor     = { 0.02f, 0.04f, 0.09f, 0.94f };
	vec4_t borderColor = { 0.00f, 0.75f, 1.00f, 0.95f };

	vec4_t factionPrimary = { 0.00f, 0.75f, 1.00f, 0.85f };
	if ( g_xpProfile.faction == FACTION_SITH ) {
		borderColor[0] = 0.95f; borderColor[1] = 0.15f; borderColor[2] = 0.15f;
		factionPrimary[0] = 0.95f; factionPrimary[1] = 0.15f; factionPrimary[2] = 0.15f;
	}

	if ( s_hWinRanks > 0 ) {
		SCR_DrawPic( cardX, cardY, cardW, cardH, s_hWinRanks );
	} else {
		SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );
	}

	// Header
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 14.0f, 6.2f, "^3STAR WARS RANK PROGRESSION TIERS", whiteColor );

	char factionStr[128];
	Com_sprintf( factionStr, sizeof(factionStr), "^7Faction Path: %s  ^7| Current Level: ^3Lvl %d",
		(g_xpProfile.faction == FACTION_SITH) ? "^1SITH EMPIRE" : "^6JEDI ORDER", g_xpProfile.level );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 36.0f, 4.8f, factionStr, whiteColor );

	// Tier List
	struct TierDef { int minLvl; int maxLvl; const char *jedi; const char *sith; };
	static TierDef tiers[8] = {
		{ 1, 24, "Youngling", "Sith Acolyte" },
		{ 25, 49, "Initiate", "Sith Hopeful" },
		{ 50, 99, "Apprentice", "Sith Apprentice" },
		{ 100, 199, "Jedi Guardian", "Sith Assassin" },
		{ 200, 349, "Jedi Knight", "Sith Warrior" },
		{ 350, 499, "Jedi Master", "Sith Lord" },
		{ 500, 749, "High Council Master", "Dark Council Master" },
		{ 750, 1000, "Grandmaster", "Sith Emperor" }
	};

	float startY = cardY + 62.0f;
	int curLvl = g_xpProfile.level;

	for ( int i = 0; i < 8; i++ ) {
		qboolean isCurrent = (curLvl >= tiers[i].minLvl && curLvl <= tiers[i].maxLvl) ? qtrue : qfalse;
		float y = startY + i * 27.0f;
		float rowX = cardX + 24.0f;
		float rowW = cardW - 48.0f;

		if ( isCurrent ) {
			vec4_t activeBg = { factionPrimary[0], factionPrimary[1], factionPrimary[2], 0.35f };
			SCR_DrawMBIICapsule( rowX, y - 2.0f, rowW, 22.0f, activeBg, borderColor );
		}

		const char *rankTitle = (g_xpProfile.faction == FACTION_SITH) ? tiers[i].sith : tiers[i].jedi;
		int reqXP = CL_XP_GetRequiredXP( tiers[i].minLvl );

		char lvlStr[32], xpStr[32];
		Com_sprintf( lvlStr, sizeof(lvlStr), "%sLvl %d-%d", isCurrent ? "^3" : "^7", tiers[i].minLvl, tiers[i].maxLvl );
		Com_sprintf( xpStr, sizeof(xpStr), "%s%d XP", isCurrent ? "^2" : "^7", reqXP );

		SCR_DrawVirtualString( rowX + 8.0f, y + 2.0f, 5.0f, lvlStr, whiteColor );
		SCR_DrawVirtualString( rowX + 130.0f, y + 2.0f, 5.0f, va("%s%s", isCurrent ? "^3" : "^7", rankTitle), whiteColor );
		SCR_DrawVirtualString( rowX + 320.0f, y + 2.0f, 5.0f, xpStr, whiteColor );

		if ( isCurrent ) {
			SCR_DrawVirtualString( rowX + 410.0f, y + 2.0f, 4.8f, "^5<-- YOU ARE HERE", whiteColor );
		}
	}

	SCR_DrawVirtualString( cardX + 24.0f, cardY + cardH - 20.0f, 4.8f, "^5Press ESC or type !ranks to close", whiteColor );
}

void SCR_DrawHelpWindowOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	float cardW = 560.0f;
	float cardH = 380.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 240.0f - cardH * 0.5f;

	vec4_t whiteColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t bgColor     = { 0.02f, 0.04f, 0.09f, 0.94f };
	vec4_t borderColor = { 0.00f, 0.75f, 1.00f, 0.95f };

	if ( g_xpProfile.faction == FACTION_SITH ) {
		borderColor[0] = 0.95f; borderColor[1] = 0.15f; borderColor[2] = 0.15f;
	}

	if ( s_hWinHelp > 0 ) {
		SCR_DrawPic( cardX, cardY, cardW, cardH, s_hWinHelp );
	} else {
		SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );
	}

	// Header
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 18.0f, 6.8f, "^3RPG MOD SYSTEM GUIDE & IN-CHAT COMMANDS", whiteColor );
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 40.0f, 4.8f, "^7Standalone Engine Client XP & Leveling System", whiteColor );

	// Section 1: XP Grants & Bonuses
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 65.0f, 6.0f, "^51. XP & Combat Bonuses", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 90.0f, 5.4f, "^3Player Kill: ^7+50 XP   ^3Duel Win: ^7+100 XP   ^3Round Win: ^7+150 XP   ^3NPC: ^7+20 XP", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 112.0f, 5.4f, "^3Perfect Duel: ^7+50 XP (0 HP loss)   ^3QuickDraw: ^7+25 XP (<3s duel win)", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 134.0f, 5.4f, "^3Multi-Kills: ^72x (+10)  3x (+20)  4x (+40)  5x (+60)  6+ (+80 XP)", whiteColor );

	// Section 2: In-Chat Commands (Works on ANY Server)
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 170.0f, 6.0f, "^52. In-Chat Commands (Works on ANY Server!)", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 195.0f, 5.4f, "^3!rpgmenu       ^7-> Open Interactive Customization Window (Mouse Support)", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 217.0f, 5.4f, "^3!stats ^7or ^3!card  ^7-> Open Profile Stats & Achievements UI Window", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 239.0f, 5.4f, "^3!ranks         ^7-> Open Rank Progression Tiers UI Window", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 261.0f, 5.4f, "^3!jedi ^7or ^3!sith  ^7-> Switch Faction Rank Title Progression Path", whiteColor );

	// Section 3: Customization CVars
	SCR_DrawVirtualString( cardX + 24.0f, cardY + 295.0f, 5.8f, "^53. HUD Customization CVars", whiteColor );
	SCR_DrawVirtualString( cardX + 32.0f, cardY + 318.0f, 5.2f, "^3/cg_rpg_hud_style 0-4 ^7(0=Holo Datapad, 1=Lightsaber, 2=Pill, 3=Imperial, 4=Neon)", whiteColor );

	SCR_DrawVirtualString( cardX + 24.0f, cardY + cardH - 22.0f, 5.0f, "^5Press ESC or type !help to close", whiteColor );
}

void SCR_DrawSettingsWindowOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;
	float cardW = 580.0f;
	float cardH = 460.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 240.0f - cardH * 0.5f;

	vec4_t whiteColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t bgColor     = { 0.02f, 0.04f, 0.09f, 0.96f };
	vec4_t borderColor = { 0.00f, 0.75f, 1.00f, 0.95f };

	if ( g_xpProfile.faction == FACTION_SITH ) {
		borderColor[0] = 0.95f; borderColor[1] = 0.15f; borderColor[2] = 0.15f;
	}

	SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );

	// Header
	SCR_DrawVirtualString( cardX + 20.0f, cardY + 12.0f, 11.5f, "^3STAR WARS RPG MENU (!rpgmenu)", whiteColor );
	SCR_DrawVirtualString( cardX + 20.0f, cardY + 30.0f, 8.5f, "^7Mouse-Interactive Customizer, Achievements & Audio Mixer", whiteColor );

	// -----------------------------------------------------------------------
	// -----------------------------------------------------------------------
	// TOP TABS BAR (Tab 0: HUD, Tab 1: Overlay, Tab 2: Achievements, Tab 3: Audio, Tab 4: Guide)
	// -----------------------------------------------------------------------
	const char *tabNames[5] = { "1. HUD Style", "2. Overlay UI", "3. Achievements", "4. Audio SFX", "5. Guide" };
	for ( int i = 0; i < 5; i++ ) {
		float tabX = cardX + 20.0f + i * 110.0f;
		float tabY = cardY + 46.0f;
		float tabW = 104.0f;
		float tabH = 26.0f;

		qboolean isSel = (g_rpgMenuTab == i) ? qtrue : qfalse;
		qboolean isHover = (g_rpgMouseX >= tabX && g_rpgMouseX <= tabX + tabW && g_rpgMouseY >= tabY && g_rpgMouseY <= tabY + tabH) ? qtrue : qfalse;

		vec4_t tabBg = { 0.04f, 0.10f, 0.20f, 0.85f };
		vec4_t tabBorder = { 0.00f, 0.60f, 0.85f, 0.70f };
		if ( isSel ) {
			tabBg[0] = 0.10f; tabBg[1] = 0.35f; tabBg[2] = 0.65f; tabBg[3] = 0.95f;
			tabBorder[0] = 1.00f; tabBorder[1] = 0.80f; tabBorder[2] = 0.20f; tabBorder[3] = 1.00f;
		} else if ( isHover ) {
			tabBg[0] = 0.08f; tabBg[1] = 0.20f; tabBg[2] = 0.40f; tabBg[3] = 0.90f;
		}

		SCR_DrawMBIICapsule( tabX, tabY, tabW, tabH, tabBg, tabBorder );
		SCR_DrawCenteredText( tabX, tabY + 5.0f, tabW, 9.0f, tabNames[i], whiteColor );
	}

	vec4_t lineCol = { 1.0f, 1.0f, 1.0f, 0.25f };
	SCR_DrawMBIICapsule( cardX + 20.0f, cardY + 76.0f, cardW - 40.0f, 1.5f, lineCol, NULL );

	qhandle_t hBtnNormal = (g_xpProfile.faction == FACTION_SITH && s_hBtnSithNormal > 0) ? s_hBtnSithNormal : s_hBtnNormal;
	qhandle_t hBtnHover  = (g_xpProfile.faction == FACTION_SITH && s_hBtnSithHover > 0) ? s_hBtnSithHover : s_hBtnHover;

	// =======================================================================
	// TAB 0: HUD STYLE & FACTION
	// =======================================================================
	if ( g_rpgMenuTab == 0 ) {
		int curStyle = cg_rpg_hud_style ? cg_rpg_hud_style->integer : 0;

		// 1. HUD Style Selector Buttons
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 90.0f, 11.0f, "^51. Select HUD Style", whiteColor );
		const char *styleNames[5] = { "0: Holo", "1: Saber", "2: Pill", "3: Imperial", "4: Neon" };
		for ( int i = 0; i < 5; i++ ) {
			float btnX = cardX + 20.0f + i * 108.0f;
			float btnY = cardY + 112.0f;
			float btnW = 102.0f;
			float btnH = 26.0f;

			qboolean isSelected = (curStyle == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
				btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 10.0f, styleNames[i], whiteColor );
		}

		// 2. Faction Path (Jedi vs Sith)
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 170.0f, 11.0f, "^52. Select Faction Progression Path", whiteColor );
		const char *factionNames[2] = { "JEDI LIGHT SIDE", "SITH DARK SIDE" };
		for ( int i = 0; i < 2; i++ ) {
			float btnX = cardX + 20.0f + i * 272.0f;
			float btnY = cardY + 192.0f;
			float btnW = 262.0f;
			float btnH = 32.0f;

			qboolean isSelected = ((i == 1 && g_xpProfile.faction == FACTION_SITH) || (i == 0 && g_xpProfile.faction == FACTION_JEDI)) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				if ( i == 1 ) {
					btnBg[0] = 0.50f; btnBg[1] = 0.08f; btnBg[2] = 0.08f; btnBg[3] = 0.95f;
					btnBorder[0] = 1.00f; btnBorder[1] = 0.20f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
				} else {
					btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
					btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
				}
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 7.0f, btnW, 11.0f, factionNames[i], whiteColor );
		}
	}
	// =======================================================================
	// TAB 1: OVERLAY UI CUSTOMIZER
	// =======================================================================
	else if ( g_rpgMenuTab == 1 ) {
		int curPos      = cg_rpg_hud_pos   ? cg_rpg_hud_pos->integer   : 0;
		int curToastPos = cg_rpg_toast_pos ? cg_rpg_toast_pos->integer : 1;
		int curNotifPos = cg_rpg_notif_pos ? cg_rpg_notif_pos->integer : 1;
		int curAvat     = cg_rpg_avatar    ? cg_rpg_avatar->integer    : 0;

		// 1. HUD Screen Position Buttons
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 86.0f, 10.5f, "^51. Select Screen Position", whiteColor );
		const char *posNames[5] = { "Top-Left", "Top-Right", "Bot-Left", "Bot-Right", "Bot-Center" };
		for ( int i = 0; i < 5; i++ ) {
			float btnX = cardX + 20.0f + i * 108.0f;
			float btnY = cardY + 104.0f;
			float btnW = 102.0f;
			float btnH = 22.0f;

			qboolean isSelected = (curPos == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
				btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 3.0f, btnW, 9.0f, posNames[i], whiteColor );
		}

		// 2. Victory UI Screen Position Buttons
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 134.0f, 10.5f, "^52. Select Victory UI Position", whiteColor );
		const char *toastPosNames[5] = { "Top-Left", "Top-Right", "Bot-Left", "Bot-Right", "Center" };
		for ( int i = 0; i < 5; i++ ) {
			float btnX = cardX + 20.0f + i * 108.0f;
			float btnY = cardY + 152.0f;
			float btnW = 102.0f;
			float btnH = 22.0f;

			qboolean isSelected = (curToastPos == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
				btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 3.0f, btnW, 9.0f, toastPosNames[i], whiteColor );
		}

		// 3. Notification Position Buttons & 4. Popups in Duels Toggle
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 182.0f, 10.5f, "^53. Select Notification Position", whiteColor );
		const char *notifPosNames[5] = { "T-Left", "T-Center", "T-Right", "B-Left", "B-Right" };
		for ( int i = 0; i < 5; i++ ) {
			float btnX = cardX + 20.0f + i * 72.0f;
			float btnY = cardY + 200.0f;
			float btnW = 66.0f;
			float btnH = 22.0f;

			qboolean isSelected = (curNotifPos == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
				btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 3.0f, btnW, 9.0f, notifPosNames[i], whiteColor );
		}

		int curDuelPopups = cg_rpg_duel_popups ? cg_rpg_duel_popups->integer : 0;
		SCR_DrawVirtualString( cardX + 388.0f, cardY + 182.0f, 10.5f, "^54. Popups in Duels", whiteColor );
		float toggleX = cardX + 388.0f;
		float toggleY = cardY + 200.0f;
		float toggleW = 172.0f;
		float toggleH = 22.0f;

		qboolean isHoverToggle = (g_rpgMouseX >= toggleX && g_rpgMouseX <= toggleX + toggleW && g_rpgMouseY >= toggleY && g_rpgMouseY <= toggleY + toggleH) ? qtrue : qfalse;
		vec4_t tBg = { 0.04f, 0.12f, 0.25f, 0.85f };
		vec4_t tBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
		if ( curDuelPopups ) {
			tBg[0] = 0.05f; tBg[1] = 0.35f; tBg[2] = 0.10f; tBg[3] = 0.95f;
			tBorder[0] = 0.20f; tBorder[1] = 0.90f; tBorder[2] = 0.30f; tBorder[3] = 1.00f;
		} else if ( isHoverToggle ) {
			tBg[0] = 0.08f; tBg[1] = 0.25f; tBg[2] = 0.50f; tBg[3] = 0.90f;
			tBorder[0] = 0.00f; tBorder[1] = 0.90f; tBorder[2] = 1.00f; tBorder[3] = 1.00f;
		}
		SCR_DrawMBIICapsule( toggleX, toggleY, toggleW, toggleH, tBg, tBorder );
		SCR_DrawCenteredText( toggleX, toggleY + 3.0f, toggleW, 9.5f, curDuelPopups ? "^2[ POPUPS ENABLED ]" : "^1[ POPUPS DISABLED ]", whiteColor );

		// 5. Avatar Selector
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 234.0f, 10.5f, "^55. Select Avatar Icon", whiteColor );
		const char *avatarNames[12] = {
			"Emblem", "Jedi Order", "Sith Empire", "Mandalorian",
			"Rebels", "Empire", "BountyHunter", "OldRepublic",
			"Custom 1", "Custom 2", "Custom 3", "Custom 4"
		};
		for ( int i = 0; i < 12; i++ ) {
			int col = i % 4;
			int row = i / 4;
			float btnX = cardX + 20.0f + col * 136.0f;
			float btnY = cardY + 252.0f + row * 26.0f;
			float btnW = 130.0f;
			float btnH = 22.0f;

			qboolean isSelected = (curAvat == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.12f, 0.25f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.65f, 0.90f, 0.75f };
			if ( isSelected ) {
				btnBg[0] = 0.10f; btnBg[1] = 0.40f; btnBg[2] = 0.75f; btnBg[3] = 0.95f;
				btnBorder[0] = 1.00f; btnBorder[1] = 0.82f; btnBorder[2] = 0.20f; btnBorder[3] = 1.00f;
			} else if ( isHover ) {
				btnBg[0] = 0.08f; btnBg[1] = 0.25f; btnBg[2] = 0.50f; btnBg[3] = 0.90f;
				btnBorder[0] = 0.00f; btnBorder[1] = 0.90f; btnBorder[2] = 1.00f; btnBorder[3] = 1.00f;
			}
			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 3.0f, btnW, 9.0f, avatarNames[i], whiteColor );
		}

		// 6. Reset Profile Stats Button
		float rX = cardX + 20.0f;
		float rY = cardY + 346.0f;
		float rW = 540.0f;
		float rH = 26.0f;
		qboolean rHover = (g_rpgMouseX >= rX && g_rpgMouseX <= rX + rW && g_rpgMouseY >= rY && g_rpgMouseY <= rY + rH) ? qtrue : qfalse;
		vec4_t rBg = { 0.40f, 0.05f, 0.05f, 0.85f };
		vec4_t rBorder = { 0.95f, 0.15f, 0.15f, 0.85f };
		if ( rHover ) { rBg[0] = 0.60f; rBg[1] = 0.08f; rBorder[0] = 1.00f; rBorder[1] = 0.30f; rBorder[2] = 0.30f; }
		SCR_DrawMBIICapsule( rX, rY, rW, rH, rBg, rBorder );
		SCR_DrawCenteredText( rX, rY + 5.0f, rW, 10.0f, "^1[ RESET ALL RPG STATS & LEVEL PROGRESS ]", whiteColor );
	}
	// =======================================================================
	// TAB 2: ACHIEVEMENTS & QUESTS
	// =======================================================================
	else if ( g_rpgMenuTab == 2 ) {
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 84.0f, 11.0f, "^5Clickable Achievement Milestones & XP Rewards", whiteColor );

		typedef struct {
			const char *title;
			const char *desc;
			int curVal;
			int maxVal;
			int claimed;
			int rewardXP;
		} achItem_t;

		achItem_t list[7] = {
			{ "Saber Master",     "Get 50 Saber Kills",    g_xpProfile.saberKills,     50, g_xpProfile.achSaberMasterClaimed,     250 },
			{ "Gunner Elite",     "Get 50 Gun Kills",      g_xpProfile.gunnerKills,    50, g_xpProfile.achGunnerEliteClaimed,     250 },
			{ "Duel Specialist",  "Win 10 Private Duels",  g_xpProfile.duelWins,       10, g_xpProfile.achDuelSpecialistClaimed,  300 },
			{ "Quick Draw Pro",   "Win 3 QuickDraw Duels", g_xpProfile.quickDrawWins,   3, g_xpProfile.achQuickDrawClaimed,       200 },
			{ "Flawless Victory", "Win 3 Perfect Duels",   g_xpProfile.perfectWins,     3, g_xpProfile.achPerfectClaimed,         250 },
			{ "Unstoppable",      "Reach 10-Kill Streak",  g_xpProfile.bestKillStreak, 10, g_xpProfile.achStreakClaimed,          400 },
			{ "Century Club",     "Reach Level 100",       g_xpProfile.level,         100, g_xpProfile.achCenturyClaimed,        1000 }
		};

		for ( int i = 0; i < 7; i++ ) {
			int col = i % 2;
			int row = i / 2;
			float itemX = cardX + 20.0f + col * 276.0f;
			float itemY = cardY + 104.0f + row * 66.0f;
			float itemW = 265.0f;
			float itemH = 58.0f;

			vec4_t achBg     = { 0.03f, 0.06f, 0.14f, 0.90f };
			vec4_t achBorder = { 0.15f, 0.35f, 0.60f, 0.80f };

			if ( list[i].claimed ) {
				achBorder[0] = 0.20f; achBorder[1] = 0.90f; achBorder[2] = 0.30f; achBorder[3] = 0.95f;
			} else if ( list[i].curVal >= list[i].maxVal ) {
				achBorder[0] = 1.00f; achBorder[1] = 0.80f; achBorder[2] = 0.20f; achBorder[3] = 1.00f;
			}

			SCR_DrawMBIICapsule( itemX, itemY, itemW, itemH, achBg, achBorder );

			// Title & Desc
			SCR_DrawVirtualString( itemX + 10.0f, itemY + 6.0f, 10.5f, va("^3%s", list[i].title), whiteColor );
			SCR_DrawVirtualString( itemX + 10.0f, itemY + 24.0f, 8.5f, va("^7%s", list[i].desc), whiteColor );

			// Claim Button / Status
			float btnX = itemX + itemW - 115.0f;
			float btnY = itemY + 26.0f;
			float btnW = 105.0f;
			float btnH = 26.0f;

			if ( list[i].claimed ) {
				SCR_DrawCenteredText( btnX, btnY + 4.0f, btnW, 9.5f, "^2[CLAIMED]", whiteColor );
			} else if ( list[i].curVal >= list[i].maxVal ) {
				qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;
				vec4_t claimBg     = { 0.10f, 0.50f, 0.15f, 0.95f };
				vec4_t claimBorder = { 0.30f, 1.00f, 0.40f, 1.00f };
				if ( isHover ) { claimBg[0] = 0.20f; claimBg[1] = 0.70f; claimBg[2] = 0.25f; }
				SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, claimBg, claimBorder );
				SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 9.0f, va("^7CLAIM +%d", list[i].rewardXP), whiteColor );
			} else {
				char progText[32];
				Com_sprintf( progText, sizeof(progText), "^7%d/%d", list[i].curVal, list[i].maxVal );
				SCR_DrawCenteredText( btnX, btnY + 4.0f, btnW, 9.5f, progText, whiteColor );
			}
		}
	}
	// =======================================================================
	// TAB 3: AUDIO & SFX MIXER
	// =======================================================================
	else if ( g_rpgMenuTab == 3 ) {
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 84.0f, 11.0f, "^5In-Game Audio SFX Mixer & Announcer Volume", whiteColor );

		// 1. Sound Volume
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 108.0f, 10.5f, "^51. Master Sound FX Volume", whiteColor );
		const char *volNames[5] = { "Mute", "25%", "50%", "75%", "100%" };
		for ( int i = 0; i < 5; i++ ) {
			float btnX = cardX + 20.0f + i * 108.0f;
			float btnY = cardY + 126.0f;
			float btnW = 102.0f;
			float btnH = 26.0f;

			qboolean isSel = (g_xpProfile.soundVolume == i) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.10f, 0.20f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.60f, 0.85f, 0.70f };
			if ( isSel ) { btnBg[0] = 0.10f; btnBg[1] = 0.45f; btnBg[2] = 0.75f; btnBorder[0] = 1.0f; btnBorder[1] = 0.8f; btnBorder[2] = 0.2f; }
			else if ( isHover ) btnBg[0] = 0.08f;

			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 9.5f, volNames[i], whiteColor );
		}

		// 2. Announcer Sounds Toggle
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 168.0f, 10.5f, "^52. Multi-Kill Announcer Voices (Double, Triple, Overkill)", whiteColor );
		for ( int i = 0; i < 2; i++ ) {
			float btnX = cardX + 20.0f + i * 272.0f;
			float btnY = cardY + 186.0f;
			float btnW = 262.0f;
			float btnH = 26.0f;

			qboolean isSel = ((i == 0 && g_xpProfile.announcerEnabled == 1) || (i == 1 && g_xpProfile.announcerEnabled == 0)) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.10f, 0.20f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.60f, 0.85f, 0.70f };
			if ( isSel ) { btnBg[0] = 0.10f; btnBg[1] = 0.45f; btnBg[2] = 0.75f; btnBorder[0] = 1.0f; btnBorder[1] = 0.8f; btnBorder[2] = 0.2f; }
			else if ( isHover ) btnBg[0] = 0.08f;

			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 10.0f, (i == 0) ? "ANNOUNCER: ENABLED" : "ANNOUNCER: DISABLED", whiteColor );
		}

		// 3. Level Up Sounds Toggle
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 228.0f, 10.5f, "^53. Level Up & Achievement Fanfare Sounds", whiteColor );
		for ( int i = 0; i < 2; i++ ) {
			float btnX = cardX + 20.0f + i * 272.0f;
			float btnY = cardY + 246.0f;
			float btnW = 262.0f;
			float btnH = 26.0f;

			qboolean isSel = ((i == 0 && g_xpProfile.levelupSndEnabled == 1) || (i == 1 && g_xpProfile.levelupSndEnabled == 0)) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.10f, 0.20f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.60f, 0.85f, 0.70f };
			if ( isSel ) { btnBg[0] = 0.10f; btnBg[1] = 0.45f; btnBg[2] = 0.75f; btnBorder[0] = 1.0f; btnBorder[1] = 0.8f; btnBorder[2] = 0.2f; }
			else if ( isHover ) btnBg[0] = 0.08f;

			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 10.0f, (i == 0) ? "LEVEL UP SOUNDS: ENABLED" : "LEVEL UP SOUNDS: DISABLED", whiteColor );
		}

		// 4. Duel Victory Sounds Toggle
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 288.0f, 10.5f, "^54. Duel Victory & QuickDraw Sounds", whiteColor );
		for ( int i = 0; i < 2; i++ ) {
			float btnX = cardX + 20.0f + i * 272.0f;
			float btnY = cardY + 306.0f;
			float btnW = 262.0f;
			float btnH = 26.0f;

			qboolean isSel = ((i == 0 && g_xpProfile.duelSndEnabled == 1) || (i == 1 && g_xpProfile.duelSndEnabled == 0)) ? qtrue : qfalse;
			qboolean isHover = (g_rpgMouseX >= btnX && g_rpgMouseX <= btnX + btnW && g_rpgMouseY >= btnY && g_rpgMouseY <= btnY + btnH) ? qtrue : qfalse;

			vec4_t btnBg = { 0.04f, 0.10f, 0.20f, 0.85f };
			vec4_t btnBorder = { 0.00f, 0.60f, 0.85f, 0.70f };
			if ( isSel ) { btnBg[0] = 0.10f; btnBg[1] = 0.45f; btnBg[2] = 0.75f; btnBorder[0] = 1.0f; btnBorder[1] = 0.8f; btnBorder[2] = 0.2f; }
			else if ( isHover ) btnBg[0] = 0.08f;

			SCR_DrawMBIICapsule( btnX, btnY, btnW, btnH, btnBg, btnBorder );
			SCR_DrawCenteredText( btnX, btnY + 5.0f, btnW, 10.0f, (i == 0) ? "DUEL SOUNDS: ENABLED" : "DUEL SOUNDS: DISABLED", whiteColor );
		}
	}
	// =======================================================================
	// TAB 4: COMMANDS & SYSTEM GUIDE
	// =======================================================================
	else if ( g_rpgMenuTab == 4 ) {
		SCR_DrawVirtualString( cardX + 20.0f, cardY + 84.0f, 11.5f, "^5In-Chat Commands (Works on ANY Server)", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 110.0f, 10.0f, "^3!rpgmenu       ^7-> Open Interactive Customization Window (Mouse Support)", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 134.0f, 10.0f, "^3!stats ^7or ^3!card  ^7-> Open Profile Stats & Achievements UI Window", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 158.0f, 10.0f, "^3!ranks         ^7-> Open Rank Progression Tiers UI Window", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 182.0f, 10.0f, "^3!jedi ^7or ^3!sith  ^7-> Switch Faction Rank Title Progression Path", whiteColor );

		SCR_DrawVirtualString( cardX + 20.0f, cardY + 220.0f, 11.5f, "^5Console CVars (For Autoexec Scripting)", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 246.0f, 9.5f, "^3/cg_rpg_hud_style 0-4 ^7(0=Holo, 1=Saber, 2=Pill, 3=Imperial, 4=Neon)", whiteColor );
		SCR_DrawVirtualString( cardX + 28.0f, cardY + 270.0f, 9.5f, "^3/cg_rpg_hud_pos 0-4   ^7(0=Top-Left, 1=Top-Right, 2=Bot-Left, 3=Bot-Right, 4=Center)", whiteColor );
	}

	SCR_DrawMBIICapsule( cardX + 20.0f, cardY + cardH - 28.0f, cardW - 40.0f, 1.5f, lineCol, NULL );
	SCR_DrawVirtualString( cardX + 20.0f, cardY + cardH - 18.0f, 9.0f, "^5Click buttons with mouse cursor or press ESC to close", whiteColor );

	// Modal Reset Confirmation Dialog Box
	if ( g_rpgResetConfirm ) {
		float boxW = 420.0f;
		float boxH = 140.0f;
		float boxX = 320.0f - boxW * 0.5f;
		float boxY = 240.0f - boxH * 0.5f;

		vec4_t mBg = { 0.08f, 0.02f, 0.02f, 0.96f };
		vec4_t mBorder = { 0.95f, 0.15f, 0.15f, 1.00f };
		SCR_DrawMBIICapsule( boxX, boxY, boxW, boxH, mBg, mBorder );

		SCR_DrawCenteredText( boxX, boxY + 14.0f, boxW, 11.5f, "^1CONFIRM STATS RESET?", whiteColor );
		SCR_DrawCenteredText( boxX, boxY + 34.0f, boxW, 9.5f, "^7Are you sure you want to reset all RPG stats and Level?", whiteColor );
		SCR_DrawCenteredText( boxX, boxY + 50.0f, boxW, 9.0f, "^7This will reset your Level, XP, Kills, Deaths & Quests to 0!", whiteColor );

		// YES Button
		float yX = boxX + 30.0f;
		float yY = boxY + 84.0f;
		float yW = 160.0f;
		float yH = 30.0f;
		qboolean yHover = (g_rpgMouseX >= yX && g_rpgMouseX <= yX + yW && g_rpgMouseY >= yY && g_rpgMouseY <= yY + yH) ? qtrue : qfalse;
		vec4_t yBg = { 0.50f, 0.08f, 0.08f, 0.95f };
		vec4_t yBdr = { 1.00f, 0.20f, 0.20f, 1.00f };
		if ( yHover ) { yBg[0] = 0.70f; yBg[1] = 0.10f; }
		SCR_DrawMBIICapsule( yX, yY, yW, yH, yBg, yBdr );
		SCR_DrawCenteredText( yX, yY + 7.0f, yW, 10.5f, "^7YES, RESET STATS", whiteColor );

		// CANCEL Button
		float cX = boxX + 230.0f;
		float cY = boxY + 84.0f;
		float cW = 160.0f;
		float cH = 30.0f;
		qboolean cHover = (g_rpgMouseX >= cX && g_rpgMouseX <= cX + cW && g_rpgMouseY >= cY && g_rpgMouseY <= cY + cH) ? qtrue : qfalse;
		vec4_t cBg = { 0.08f, 0.25f, 0.50f, 0.90f };
		vec4_t cBdr = { 0.00f, 0.90f, 1.00f, 1.00f };
		if ( cHover ) { cBg[0] = 0.12f; cBg[1] = 0.40f; }
		SCR_DrawMBIICapsule( cX, cY, cW, cH, cBg, cBdr );
		SCR_DrawCenteredText( cX, cY + 7.0f, cW, 10.5f, "^7CANCEL", whiteColor );
	}

	// Render Holographic Mouse Cursor Pointer
	if ( (!s_hMousePointer || s_hMousePointer == 0) && re && re->RegisterShaderNoMip ) {
		s_hMousePointer = re->RegisterShaderNoMip( "gfx/rpg_hud/mouse_pointer" );
	}
	float curX = g_rpgMouseX;
	float curY = g_rpgMouseY;
	if ( s_hMousePointer > 0 ) {
		SCR_DrawPic( curX - 12.0f, curY - 12.0f, 24.0f, 24.0f, s_hMousePointer );
	} else {
		vec4_t curColor = { 1.00f, 0.85f, 0.20f, 0.95f };
		SCR_FillRect( curX - 4.0f, curY - 1.0f, 9.0f, 2.0f, curColor );
		SCR_FillRect( curX - 1.0f, curY - 4.0f, 2.0f, 9.0f, curColor );
		vec4_t curDot = { 1.00f, 1.00f, 1.00f, 1.00f };
		SCR_FillRect( curX - 1.0f, curY - 1.0f, 2.0f, 2.0f, curDot );
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
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	if ( !cg_drawStats || !cg_drawStats->integer ) {
		return;
	}

	// Modal Window Dimensions (Centered 420x240 card layout)
	float winW = 420.0f;
	float winH = 240.0f;
	float winX = 320.0f - winW * 0.5f;
	float winY = 120.0f;

	vec4_t borderColor = { 0.00f, 0.70f, 1.00f, 0.85f };

	if ( !s_hBox ) s_hBox = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
	if ( s_hBox ) {
		SCR_DrawPic( winX, winY, winW, winH, s_hBox );
	} else {
		vec4_t bgColor     = { 0.03f, 0.06f, 0.12f, 0.90f };
		SCR_DrawRoundedGlassPanel( winX, winY, winW, winH, 6.0f, bgColor, borderColor );
	}

	vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.88f };
	SCR_DrawRoundedGlassPanel( winX + 4.0f, winY + 4.0f, winW - 8.0f, 22.0f, 3.0f, headerBg, NULL );

	// Title
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( winX + 115.0f, winY + 6.0f, 6.0f, "^3CHARACTER STATISTICS", yellowCol );

	// Close Button instruction
	SCR_DrawVirtualString( winX + winW - 55.0f, winY + 6.0f, 5.0f, "^1[ESC]", yellowCol );

	// LEFT COLUMN - Profile Picture & Title card
	float avatarX = winX + 15.0f;
	float avatarY = winY + 38.0f;
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
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 8.0f, 5.0f, lvlStr, whiteColor );

	// Display Name
	char dName[64];
	Com_sprintf( dName, sizeof(dName), "^7%.32s", g_rpgStats.displayName );
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 22.0f, 4.8f, dName, whiteColor );

	// Rank Title
	char rTitle[64];
	Com_sprintf( rTitle, sizeof(rTitle), "^3%.16s", g_rpgStats.rankTitle );
	SCR_DrawVirtualString( avatarX, avatarY + avatarSize + 34.0f, 4.5f, rTitle, whiteColor );

	// Divider line between columns
	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.35f };
	SCR_FillRect( winX + 115.0f, winY + 34.0f, 1.0f, winH - 52.0f, divColor );

	// RIGHT COLUMN - Statistics Rows
	float rightX = winX + 125.0f;
	float rightY = winY + 38.0f;

	// Row 1: XP Progress Bar
	SCR_DrawVirtualString( rightX, rightY, 4.8f, "^5XP Progress:", whiteColor );
	float barX = rightX + 75.0f;
	float barY = rightY + 1.0f;
	float barW = 100.0f;
	float barH = 10.0f;
	vec4_t barBg = { 0.02f, 0.04f, 0.08f, 0.90f };
	SCR_FillRect( barX, barY, barW, barH, barBg );

	int xpVal = g_rpgStats.xp % 1000;
	float xpRatio = (float)xpVal / 1000.0f;
	if ( xpRatio < 0.0f ) xpRatio = 0.0f;
	if ( xpRatio > 1.0f ) xpRatio = 1.0f;
	if ( xpRatio > 0.0f ) {
		vec4_t barFill = { 0.00f, 0.70f, 1.00f, 0.95f };
		SCR_FillRect( barX + 1.5f, barY + 1.5f, (barW - 3.0f) * xpRatio, barH - 3.0f, barFill );
	}
	char xpLabel[32];
	Com_sprintf( xpLabel, sizeof(xpLabel), "^2%d^7/^21000 XP", xpVal );
	SCR_DrawVirtualString( barX + barW + 5.0f, rightY, 4.8f, xpLabel, whiteColor );

	// Row 2: Force Rating (ELO)
	SCR_DrawVirtualString( rightX, rightY + 18.0f, 4.8f, va( "^5Force Rating: ^7%d FR", g_rpgStats.fr ), whiteColor );

	// Row 3: Credits Balance
	SCR_DrawVirtualString( rightX, rightY + 36.0f, 4.8f, va( "^5Credits: ^7%d Credits", g_rpgStats.credits ), whiteColor );

	// Row 4: Wins / Losses
	float wlRatio = g_rpgStats.losses > 0 ? (float)g_rpgStats.wins / (float)g_rpgStats.losses : (float)g_rpgStats.wins;
	SCR_DrawVirtualString( rightX, rightY + 54.0f, 4.8f, va( "^5Wins / Losses: ^2%d^7/^1%d ^5(Ratio: %.2f)", g_rpgStats.wins, g_rpgStats.losses, wlRatio ), whiteColor );

	// Row 5: Kills / Deaths
	float kdRatio = g_rpgStats.deaths > 0 ? (float)g_rpgStats.kills / (float)g_rpgStats.deaths : (float)g_rpgStats.kills;
	SCR_DrawVirtualString( rightX, rightY + 72.0f, 4.8f, va( "^5Kills / Deaths: ^2%d^7/^1%d ^5(K/D: %.2f)", g_rpgStats.kills, g_rpgStats.deaths, kdRatio ), whiteColor );

	// Row 6: Kill Streak
	SCR_DrawVirtualString( rightX, rightY + 90.0f, 4.8f, va( "^5Kill Streak: ^7Current: ^2%d ^7| Highest: ^3%d", g_rpgStats.curStreak, g_rpgStats.highStreak ), whiteColor );

	// Row 7: Trivia Wins
	SCR_DrawVirtualString( rightX, rightY + 108.0f, 4.8f, va( "^5Trivia Wins: ^7%d Wins", g_rpgStats.triviaWins ), whiteColor );

	// Row 8: Main Rival
	char rivalStr[128];
	if ( g_rpgStats.rivalCount > 0 ) {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^5Main Rival: ^7%.16s ^5(%d duels)", g_rpgStats.rivalName, g_rpgStats.rivalCount );
	} else {
		Com_sprintf( rivalStr, sizeof(rivalStr), "^5Main Rival: ^7None" );
	}
	SCR_DrawVirtualString( rightX, rightY + 126.0f, 4.8f, rivalStr, whiteColor );

	// Footer instruction
	SCR_DrawVirtualString( winX + 90.0f, winY + winH - 12.0f, 4.5f, "^7Press ^3F8^7, ^3ESC^7, or type ^3!stats^7 to close", whiteColor );
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
	if ( cg_rpg_duel_popups && cg_rpg_duel_popups->integer == 0 ) return;
	if ( cg_rpg_notify_popups && cg_rpg_notify_popups->integer == 0 ) return;

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
	const float panelH = 58.0f;
	float defaultX = 640.0f - panelW - 14.0f;
	float defaultY = 125.0f;

	if ( cg_rpg_toast_pos ) {
		int posVal = cg_rpg_toast_pos->integer;
		if ( posVal == 0 ) { defaultX = 14.0f; defaultY = 125.0f; }
		else if ( posVal == 1 ) { defaultX = 640.0f - panelW - 14.0f; defaultY = 125.0f; }
		else if ( posVal == 2 ) { defaultX = 14.0f; defaultY = 280.0f; }
		else if ( posVal == 3 ) { defaultX = 640.0f - panelW - 14.0f; defaultY = 280.0f; }
		else if ( posVal == 4 ) { defaultX = 320.0f - panelW * 0.5f; defaultY = 100.0f; }
	}

	const float panelX = defaultX;
	const float panelY = defaultY;

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
	char statsStr[96];
	if ( g_rpgToast.isWin ) {
		Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp );
	} else {
		Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^1%d ^7| ^6+%d CR", g_rpgToast.eloDelta, g_rpgToast.credits );
	}
	SCR_DrawVirtualString( textX, panelY + 26.0f, 4.0f, statsStr, whiteA );

	// Row 4: Captured HP/BP remaining values
	char bpStr[96];
	if ( g_rpgToast.isWin ) {
		if ( g_rpgToast.killerHP > 0 || g_rpgToast.killerBP > 0 ) {
			Com_sprintf( bpStr, sizeof( bpStr ), "^7My Left: ^2%d HP ^7/ ^5%d BP", g_rpgToast.killerHP, g_rpgToast.killerBP );
		} else {
			Com_sprintf( bpStr, sizeof( bpStr ), "^7My Left: ^5Unknown BP" );
		}
	} else {
		if ( g_rpgToast.killerHP > 0 || g_rpgToast.killerBP > 0 ) {
			Com_sprintf( bpStr, sizeof( bpStr ), "^7Opp Left: ^1%d HP ^7/ ^5%d BP", g_rpgToast.killerHP, g_rpgToast.killerBP );
		} else {
			Com_sprintf( bpStr, sizeof( bpStr ), "^7Opp Left: ^5Unknown BP" );
		}
	}
	SCR_DrawVirtualString( textX, panelY + 37.0f, 3.8f, bpStr, whiteA );

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

	// Draw see-through glass background panel behind inspect text
	float padX = 8.0f;
	float boxX = x - padX;
	float boxW = w + padX * 2.0f;
	float boxY = y - 3.0f;
	float boxH = 16.0f;

	vec4_t glassBg = { 0.03f, 0.06f, 0.12f, 0.45f * alpha };
	vec4_t glassBorder = { 0.10f, 0.60f, 0.90f, 0.35f * alpha };
	SCR_DrawRoundedGlassPanel( boxX, boxY, boxW, boxH, 3.0f, glassBg, glassBorder );

	// Make the text transparent (max 0.75 alpha)
	vec4_t whiteA = { 1.0f, 1.0f, 1.0f, alpha * 0.75f };
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

	static qhandle_t s_hWantedBg = 0;
	static qboolean  s_hWantedBgTried = qfalse;
	if ( !s_hWantedBgTried ) {
		s_hWantedBgTried = qtrue;
		if ( re && re->RegisterShader ) {
			s_hWantedBg = re->RegisterShader( "gfx/rpg_hud/wanted_bg" );
		}
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
			SCR_DrawRPGHUDOverlay();
			SCR_DrawDemoRecording();
			SCR_DrawLeaderboardOverlay();
			SCR_DrawStatsOverlay();
			SCR_DrawToastOverlay();
			SCR_DrawInspectOverlay();
			SCR_DrawBountyOverlay();
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
