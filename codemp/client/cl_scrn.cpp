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
	cg_rpg_level = Cvar_Get ("cg_rpg_level", "1", 0);
	cg_rpg_xp = Cvar_Get ("cg_rpg_xp", "0", 0);
	cg_rpg_xp_max = Cvar_Get ("cg_rpg_xp_max", "1000", 0);
	cg_rpg_fr = Cvar_Get ("cg_rpg_fr", "1000", 0);
	cg_rpg_avatar = Cvar_Get ("cg_rpg_avatar", "gfx/hud/avatar_default", 0);
	cg_rpg_name = Cvar_Get ("cg_rpg_name", "", 0);
	cg_rpg_rank = Cvar_Get ("cg_rpg_rank", "Padawan", 0);
	cg_drawLeaderboard = Cvar_Get ("cg_drawLeaderboard", "0", 0);

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

void SCR_DrawRPGHUDOverlay( void ) {
	if ( cls.state != CA_ACTIVE ) {
		return;
	}
	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Flush handles on map/state transitions to prevent stale renderer pointer crashes
	if ( cls.state != s_lastState ) {
		s_hBox = 0;
		s_hBarBg = 0;
		s_hBarFill = 0;
		s_hAvatar = 0;
		s_hAvatarFrame = 0;
		s_hModalBg = 0;
		s_lastState = cls.state;
	}

	// Register HD TGA Shaders dynamically once active
	if ( !s_hBox ) s_hBox = re->RegisterShader( "gfx/rpg_hud/panel_bg" );
	if ( !s_hBarBg ) s_hBarBg = re->RegisterShader( "gfx/rpg_hud/bar_bg" );
	if ( !s_hBarFill ) s_hBarFill = re->RegisterShader( "gfx/rpg_hud/bar_fill" );
	if ( !s_hAvatar ) s_hAvatar = re->RegisterShader( "gfx/rpg_hud/avatar_default" );
	if ( !s_hAvatarFrame ) s_hAvatarFrame = re->RegisterShader( "gfx/rpg_hud/avatar_frame" );
	if ( !s_hModalBg ) s_hModalBg = re->RegisterShader( "gfx/rpg_hud/leaderboard_bg" );

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

		// Line 2: Rank Title & Force Rating ELO
		char rankStr[96];
		Com_sprintf( rankStr, sizeof(rankStr), "^3%.18s ^7|^2 %d", rankTitle, fr );
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
	char nameStr[96];
	Com_sprintf( nameStr, sizeof(nameStr), "^7%.18s", playerName );
	SCR_DrawVirtualString( textX, panelY + 4.0f, 5.2f, nameStr, whiteColor );

	// Line 2: Rank Title & Force Rating ELO
	char rankStr[96];
	Com_sprintf( rankStr, sizeof(rankStr), "^3%.18s ^7|^2 %d", rankTitle, fr );
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

//=======================================================

/*
==================
SCR_DrawScreenField

This will be called twice if rendering in stereo mode
==================
*/
void SCR_DrawScreenField( stereoFrame_t stereoFrame ) {
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
