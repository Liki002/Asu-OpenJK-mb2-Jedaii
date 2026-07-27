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
rpgPlayerStats_t g_rpgStats;
rpgToastNotif_t  g_rpgToast   = {qfalse, qfalse, 0, 0, 0, "", 0};
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
static qboolean  s_shadersTried = qfalse;

void SCR_DrawRPGHUDOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}

	CL_XP_CheckGameEvents();

	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Register HD TGA Shaders dynamically once active
	if ( !s_shadersTried ) {
		s_shadersTried = qtrue;
		if ( re && re->RegisterShader ) {
			s_hBox         = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
			s_hBarBg       = re->RegisterShader( "gfx/rpg_hud/bar_bg" );
			s_hBarFill     = re->RegisterShader( "gfx/rpg_hud/bar_fill" );
			s_hAvatar      = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
			s_hAvatarFrame = re->RegisterShader( "gfx/rpg_hud/avatar_frame" );
			s_hModalBg     = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );
		}
	}

	int style = cg_rpg_style ? cg_rpg_style->integer : 0;
	float panelW = (style == 1) ? 175.0f : 140.0f;
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

	cvar_t *clName = Cvar_Get( "name", "Padawan", 0 );
	const char *playerName = (cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : (clName ? clName->string : "Player");
	const char *rankTitle = (cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : "Padawan";
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
		char nameLvlStr[96];
		Com_sprintf( nameLvlStr, sizeof(nameLvlStr), "^7%.22s ^3Lv %d", playerName, level );
		SCR_DrawVirtualString( textX, panelY + 2.0f, 5.2f, nameLvlStr, whiteColor );

		// Line 2: Rank Title & Private Duel Record (Wins - Losses)
		char rankStr[96];
		Com_sprintf( rankStr, sizeof(rankStr), "^3%.18s ^7|^2 %dW-%dL", rankTitle, g_xpProfile.duelWins, g_xpProfile.duelLosses );
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
				vec4_t factionFill = { 0.00f, 0.70f, 0.95f, 0.95f };
				if ( g_xpProfile.faction == FACTION_SITH ) {
					factionFill[0] = 0.95f; factionFill[1] = 0.15f; factionFill[2] = 0.15f;
				}
				SCR_DrawMBIICapsule( fillX, fillY, fillW, fillH, factionFill, NULL );
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
	char nameStr[96];
	Com_sprintf( nameStr, sizeof(nameStr), "^7%.18s", playerName );
	SCR_DrawVirtualString( textX, panelY + 4.0f, 5.2f, nameStr, whiteColor );

	// Line 2: Rank Title & Private Duel Record (Wins - Losses)
	char rankStr[96];
	Com_sprintf( rankStr, sizeof(rankStr), "^3%.18s ^7|^2 %dW-%dL", rankTitle, g_xpProfile.duelWins, g_xpProfile.duelLosses );
	SCR_DrawVirtualString( textX, panelY + 16.0f, 4.3f, rankStr, whiteColor );

	// Line 3: Dynamic XP Progress Bar (TGA Background)
	float barX = textX;
	float barY = panelY + 28.5f;
	float barW = panelX + panelW - barX - 5.0f;
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

	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", (int)s_visualXP, xpMax );
	float textWidthPixels = (strlen(xpText) * 3.8f * 0.60f);
	float xpTextX = barX + barW - textWidthPixels - 3.0f;
	if ( xpTextX < barX + 3.0f ) xpTextX = barX + 3.0f;
	SCR_DrawVirtualString( xpTextX, barY + 1.5f, 3.8f, xpText, whiteColor );

	if ( g_xpDrawCard ) {
		SCR_DrawProfileCardOverlay();
	}
}

void SCR_DrawProfileCardOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) return;

	float cardW = 340.0f;
	float cardH = 210.0f;
	float cardX = 320.0f - cardW * 0.5f;
	float cardY = 240.0f - cardH * 0.5f;

	vec4_t whiteColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
	vec4_t bgColor     = { 0.02f, 0.04f, 0.08f, 0.92f };
	vec4_t borderColor = { 0.00f, 0.70f, 0.95f, 0.95f };

	if ( g_xpProfile.faction == FACTION_SITH ) {
		borderColor[0] = 0.95f; borderColor[1] = 0.15f; borderColor[2] = 0.15f;
	}

	SCR_DrawMBIICapsule( cardX, cardY, cardW, cardH, bgColor, borderColor );

	char headerStr[128];
	Com_sprintf( headerStr, sizeof(headerStr), "^7%.20s ^3[Lvl %d %s^3]",
		CL_XP_GetProfileName(), g_xpProfile.level, (g_xpProfile.faction == FACTION_SITH) ? "^1SITH" : "^6JEDI" );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 14.0f, 5.8f, headerStr, whiteColor );

	const char *rankTitle = CL_XP_GetRankTitle( g_xpProfile.level, g_xpProfile.faction );
	char rankSub[64];
	Com_sprintf( rankSub, sizeof(rankSub), "^3Rank Title: ^7%s", rankTitle );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 34.0f, 4.8f, rankSub, whiteColor );

	vec4_t lineCol = { 1.0f, 1.0f, 1.0f, 0.25f };
	SCR_DrawMBIICapsule( cardX + 16.0f, cardY + 48.0f, cardW - 32.0f, 1.5f, lineCol, NULL );

	int curXP = 0, reqXP = 0;
	float percent = 0.0f;
	CL_XP_GetLevelProgress( &curXP, &reqXP, &percent );

	char xpStr[96];
	Com_sprintf( xpStr, sizeof(xpStr), "^7XP Progress: ^2%d ^7/ ^2%d XP ^7(%.1f%%)", curXP, reqXP, percent * 100.0f );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 56.0f, 4.2f, xpStr, whiteColor );

	float kdRatio = (g_xpProfile.deaths > 0) ? ((float)g_xpProfile.kills / (float)g_xpProfile.deaths) : (float)g_xpProfile.kills;
	char kdStr[96];
	Com_sprintf( kdStr, sizeof(kdStr), "^7Combat Kills: ^3%d  ^7Deaths: ^1%d  ^7K/D: ^2%.2f", g_xpProfile.kills, g_xpProfile.deaths, kdRatio );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 85.0f, 4.5f, kdStr, whiteColor );

	int totalDuels = g_xpProfile.duelWins + g_xpProfile.duelLosses;
	float duelWinRate = (totalDuels > 0) ? (((float)g_xpProfile.duelWins / (float)totalDuels) * 100.0f) : 0.0f;
	char duelStr[96];
	Com_sprintf( duelStr, sizeof(duelStr), "^7Private Duels: ^2%dW ^7- ^1%dL  ^7Win Rate: ^3%.1f%%", g_xpProfile.duelWins, g_xpProfile.duelLosses, duelWinRate );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 107.0f, 4.5f, duelStr, whiteColor );

	char npcTotalStr[96];
	Com_sprintf( npcTotalStr, sizeof(npcTotalStr), "^7NPC Kills: ^3%d  ^7Total Lifetime XP: ^3%d", g_xpProfile.npcKills, g_xpProfile.xp );
	SCR_DrawVirtualString( cardX + 16.0f, cardY + 129.0f, 4.5f, npcTotalStr, whiteColor );

	SCR_DrawVirtualString( cardX + 16.0f, cardY + cardH - 18.0f, 3.8f, "^5Type /rpg_card to toggle this profile card", whiteColor );
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
	char statsStr[96];
	if ( g_rpgToast.isWin ) {
		Com_sprintf( statsStr, sizeof( statsStr ), "^5ELO ^2+%d ^7| ^6+%d CR ^7| ^3+%d XP", g_rpgToast.eloDelta, g_rpgToast.credits, g_rpgToast.xp );
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
