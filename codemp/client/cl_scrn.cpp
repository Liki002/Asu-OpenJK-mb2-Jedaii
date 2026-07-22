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
** chars are drawn at 640*480 virtual screen size
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
	aw = size;
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
SCR_Init
==================
*/
cvar_t		*cg_rpg_pos;

/*
==================
SCR_RPGHUDPos_f

Console command: rpg_hud_pos <left|right|bottomright|bottomleft>
==================
*/
static void SCR_RPGHUDPos_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "^3Usage: ^7rpg_hud_pos <left | right | bottomright | bottomleft>\n" );
		Com_Printf( "^3Current position: ^7%s (x: %.0f, y: %.0f)\n",
			(cg_rpg_pos && cg_rpg_pos->string[0]) ? cg_rpg_pos->string : "left",
			cg_rpg_x ? cg_rpg_x->value : 16.0f,
			cg_rpg_y ? cg_rpg_y->value : 16.0f );
		return;
	}

	const char *pos = Cmd_Argv( 1 );
	if ( !Q_stricmp( pos, "left" ) || !Q_stricmp( pos, "topleft" ) ) {
		Cvar_Set( "cg_rpg_pos", "left" );
		Cvar_Set( "cg_rpg_x", "16" );
		Cvar_Set( "cg_rpg_y", "16" );
		Com_Printf( "^2RPG HUD set to TOP LEFT (16, 16)\n" );
	} else if ( !Q_stricmp( pos, "right" ) || !Q_stricmp( pos, "topright" ) ) {
		Cvar_Set( "cg_rpg_pos", "right" );
		Cvar_Set( "cg_rpg_x", "418" );
		Cvar_Set( "cg_rpg_y", "16" );
		Com_Printf( "^2RPG HUD set to TOP RIGHT (418, 16)\n" );
	} else if ( !Q_stricmp( pos, "bottomright" ) ) {
		Cvar_Set( "cg_rpg_pos", "bottomright" );
		Cvar_Set( "cg_rpg_x", "418" );
		Cvar_Set( "cg_rpg_y", "345" );
		Com_Printf( "^2RPG HUD set to BOTTOM RIGHT (418, 345)\n" );
	} else if ( !Q_stricmp( pos, "bottomleft" ) ) {
		Cvar_Set( "cg_rpg_pos", "bottomleft" );
		Cvar_Set( "cg_rpg_x", "16" );
		Cvar_Set( "cg_rpg_y", "345" );
		Com_Printf( "^2RPG HUD set to BOTTOM LEFT (16, 345)\n" );
	} else {
		Com_Printf( "^1Unknown position '%s'. Use ^3left^1, ^3right^1, ^3bottomright^1, or ^3bottomleft^1.\n", pos );
	}
}

void SCR_Init( void ) {
	cl_timegraph = Cvar_Get ("timegraph", "0", CVAR_CHEAT);
	cl_debuggraph = Cvar_Get ("debuggraph", "0", CVAR_CHEAT);
	cl_graphheight = Cvar_Get ("graphheight", "32", CVAR_CHEAT);
	cl_graphscale = Cvar_Get ("graphscale", "1", CVAR_CHEAT);
	cl_graphshift = Cvar_Get ("graphshift", "0", CVAR_CHEAT);

	cg_drawRPGHUD = Cvar_Get ("cg_drawRPGHUD", "1", CVAR_ARCHIVE);
	cg_rpg_pos = Cvar_Get ("cg_rpg_pos", "left", CVAR_ARCHIVE);
	cg_rpg_x = Cvar_Get ("cg_rpg_x", "16", CVAR_ARCHIVE);
	cg_rpg_y = Cvar_Get ("cg_rpg_y", "16", CVAR_ARCHIVE);
	cg_rpg_level = Cvar_Get ("cg_rpg_level", "1", 0);
	cg_rpg_xp = Cvar_Get ("cg_rpg_xp", "0", 0);
	cg_rpg_xp_max = Cvar_Get ("cg_rpg_xp_max", "1000", 0);
	cg_rpg_fr = Cvar_Get ("cg_rpg_fr", "1000", 0);
	cg_rpg_avatar = Cvar_Get ("cg_rpg_avatar", "gfx/hud/avatar_default", 0);
	cg_rpg_name = Cvar_Get ("cg_rpg_name", "", 0);
	cg_rpg_rank = Cvar_Get ("cg_rpg_rank", "Padawan", 0);
	cg_drawLeaderboard = Cvar_Get ("cg_drawLeaderboard", "0", 0);

	Cmd_AddCommand( "rpg_hud_pos", SCR_RPGHUDPos_f, "Position RPG HUD: left, right, bottomright, bottomleft" );

	scr_initialized = qtrue;
}

/*
==================
SCR_FillRoundedRect

Draws a rectangle with smooth rounded corners in 640x480 virtual coordinates
==================
*/
static void SCR_FillRoundedRect( float x, float y, float width, float height, float r, const float *color ) {
	if ( width <= 0 || height <= 0 ) return;

	// Center body
	SCR_FillRect( x + r, y, width - 2.0f * r, height, color );
	// Left & right wings
	SCR_FillRect( x, y + r, r, height - 2.0f * r, color );
	SCR_FillRect( x + width - r, y + r, r, height - 2.0f * r, color );

	// Corner fills (inset to give soft rounded corner curve)
	float hr = r * 0.5f;
	SCR_FillRect( x + hr, y + hr, hr, hr, color );
	SCR_FillRect( x + width - r, y + hr, hr, hr, color );
	SCR_FillRect( x + hr, y + height - r, hr, hr, color );
	SCR_FillRect( x + width - r, y + height - r, hr, hr, color );
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
		xx += (charSize * 0.68f);
		s++;
	}
	re->SetColor( NULL );
}

/*
==================
SCR_DrawRPGHUDOverlay

Renders compact client-side RPG HUD Overlay
==================
*/
static float s_visualXP = -1.0f;

void SCR_DrawRPGHUDOverlay( void ) {
	if ( !cg_drawRPGHUD || !cg_drawRPGHUD->integer ) {
		return;
	}

	// Calculate preset position defaults if custom X/Y are not set
	float defaultX = 16.0f;
	float defaultY = 16.0f;
	if ( cg_rpg_pos && cg_rpg_pos->string[0] ) {
		if ( !Q_stricmp( cg_rpg_pos->string, "right" ) || !Q_stricmp( cg_rpg_pos->string, "topright" ) ) {
			defaultX = 418.0f; defaultY = 16.0f;
		} else if ( !Q_stricmp( cg_rpg_pos->string, "bottomright" ) ) {
			defaultX = 418.0f; defaultY = 345.0f;
		} else if ( !Q_stricmp( cg_rpg_pos->string, "bottomleft" ) ) {
			defaultX = 16.0f; defaultY = 345.0f;
		}
	}

	// Dynamic position & clean dimensions (virtual 640x480 coordinates)
	float panelX = (cg_rpg_x && cg_rpg_x->value != 0.0f) ? cg_rpg_x->value : defaultX;
	float panelY = (cg_rpg_y && cg_rpg_y->value != 0.0f) ? cg_rpg_y->value : defaultY;
	float panelW = 205.0f;
	float panelH = 48.0f;

	// Sleek dark glass panel with a crisp 1px cyan border line (Zero yellow texture bleed)
	vec4_t bgColor     = { 0.03f, 0.06f, 0.12f, 0.90f };
	vec4_t borderColor = { 0.00f, 0.65f, 0.95f, 0.80f };

	SCR_FillRect( panelX, panelY, panelW, panelH, bgColor );
	SCR_FillRect( panelX, panelY, panelW, 1.0f, borderColor );                 // Top
	SCR_FillRect( panelX, panelY + panelH - 1.0f, panelW, 1.0f, borderColor );     // Bottom
	SCR_FillRect( panelX, panelY, 1.0f, panelH, borderColor );                 // Left
	SCR_FillRect( panelX + panelW - 1.0f, panelY, 1.0f, panelH, borderColor );     // Right

	// Avatar Box (25x25)
	float avatarX = panelX + 4.0f;
	float avatarY = panelY + 4.0f;
	float avatarSize = 25.0f;

	vec4_t avatarBg = { 0.08f, 0.14f, 0.26f, 0.80f };
	SCR_FillRect( avatarX, avatarY, avatarSize, avatarSize, avatarBg );

	qboolean avatarDrawn = qfalse;
	const char *avatarPaths[8] = {
		(cg_rpg_avatar && cg_rpg_avatar->string[0]) ? cg_rpg_avatar->string : "gfx/hud/avatar_default",
		"gfx/hud/avatar_default",
		"gfx/hud/avatar_default.jpg",
		"gfx/hud/avatar_default.tga",
		"gfx/hud/avatar_sith.jpg",
		"gfx/hud/avatar_sith.tga",
		"gfx/rpg/avatar_default",
		"gfx/2d/logos/mb_jedaii"
	};

	for ( int i = 0; i < 8; i++ ) {
		if ( !avatarPaths[i] || !avatarPaths[i][0] ) continue;
		qhandle_t hAvatar = re->RegisterShader( avatarPaths[i] );
		if ( hAvatar ) {
			SCR_DrawPic( avatarX, avatarY, avatarSize, avatarSize, hAvatar );
			avatarDrawn = qtrue;
			break;
		}
	}

	// Fallback procedural vector crest
	if ( !avatarDrawn ) {
		vec4_t crestBg = { 0.06f, 0.12f, 0.24f, 0.90f };
		SCR_FillRect( avatarX + 1.0f, avatarY + 1.0f, avatarSize - 2.0f, avatarSize - 2.0f, crestBg );

		vec4_t emblemGold = { 0.95f, 0.80f, 0.20f, 0.95f };
		vec4_t emblemCyan = { 0.20f, 0.85f, 1.00f, 0.95f };
		float cx = avatarX + avatarSize * 0.5f;
		float cy = avatarY + avatarSize * 0.5f;

		SCR_FillRect( cx - 1.0f, cy - 7.0f, 2.0f, 14.0f, emblemCyan );
		SCR_FillRect( cx - 6.0f, cy - 2.0f, 12.0f, 2.0f, emblemGold );
		SCR_FillRect( cx - 4.0f, cy + 2.0f, 8.0f, 2.0f, emblemGold );
	}

	// Level Badge (placed under Avatar frame)
	int level = cg_rpg_level ? cg_rpg_level->integer : 1;
	char levelStr[32];
	Com_sprintf( levelStr, sizeof(levelStr), "^3Lv %d", level );
	float levelX = avatarX + 1.0f;
	float levelY = avatarY + avatarSize + 2.0f;
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( levelX, levelY, 5.0f, levelStr, whiteColor );

	// Right Content Column (Name, Rank | FR, XP Bar)
	float textX = avatarX + avatarSize + 6.0f;
	cvar_t *clName = Cvar_Get( "name", "Padawan", 0 );
	const char *playerName = (cg_rpg_name && cg_rpg_name->string[0]) ? cg_rpg_name->string : (clName ? clName->string : "Player");
	const char *rankTitle = (cg_rpg_rank && cg_rpg_rank->string[0]) ? cg_rpg_rank->string : "Padawan";
	int fr = cg_rpg_fr ? cg_rpg_fr->integer : 1000;

	// Line 1: Player Name (Full name up to 32 characters)
	char nameStr[96];
	Com_sprintf( nameStr, sizeof(nameStr), "^7%.32s", playerName );
	SCR_DrawVirtualString( textX, panelY + 4.0f, 5.5f, nameStr, whiteColor );

	// Line 2: Rank Title & Force Rating ELO (Full rank titles up to 24 characters)
	char rankStr[96];
	Com_sprintf( rankStr, sizeof(rankStr), "^3%.24s ^7|^2 %d FR", rankTitle, fr );
	SCR_DrawVirtualString( textX, panelY + 16.0f, 4.8f, rankStr, whiteColor );

	// Line 3: Dynamic XP Progress Bar & Smooth Animated Fill
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

	float barX = textX;
	float barY = panelY + 29.0f;
	float barW = panelX + panelW - barX - 5.0f;
	float barH = 12.0f;

	// Progress Bar Container
	vec4_t barBorder = { 0.00f, 0.60f, 0.95f, 0.65f };
	vec4_t barBg     = { 0.02f, 0.04f, 0.08f, 0.90f };
	SCR_FillRect( barX, barY, barW, barH, barBg );
	SCR_FillRect( barX, barY, barW, 1.0f, barBorder );
	SCR_FillRect( barX, barY + barH - 1.0f, barW, 1.0f, barBorder );
	SCR_FillRect( barX, barY, 1.0f, barH, barBorder );
	SCR_FillRect( barX + barW - 1.0f, barY, 1.0f, barH, barBorder );

	// Dynamic Fill Bar
	float fillX = barX + 1.0f;
	float fillY = barY + 1.0f;
	float maxFillW = barW - 2.0f;
	float fillW = maxFillW * xpRatio;
	float fillH = barH - 2.0f;

	if ( fillW > 0.0f ) {
		vec4_t cyanFill = { 0.00f, 0.70f, 0.95f, 0.95f };
		SCR_FillRect( fillX, fillY, fillW, fillH, cyanFill );
	}

	// XP Numeric Readout (Green text over progress bar)
	char xpText[64];
	Com_sprintf( xpText, sizeof(xpText), "^2%d^7/^2%d XP", (int)s_visualXP, xpMax );
	float textWidthPixels = (strlen(xpText) * 4.5f * 0.68f);
	float xpTextX = barX + barW - textWidthPixels - 3.0f;
	if ( xpTextX < barX + 3.0f ) xpTextX = barX + 3.0f;
	SCR_DrawVirtualString( xpTextX, barY + 2.0f, 4.5f, xpText, whiteColor );
}


topLeaderboardEntry_t g_topLeaderboard[10];
int g_topLeaderboardCount = 0;

/*
==================
SCR_DrawLeaderboardOverlay

Renders sleek modal popup window showing top 10 ranked players
==================
*/
void SCR_DrawLeaderboardOverlay( void ) {
	if ( !cg_drawLeaderboard || !cg_drawLeaderboard->integer ) {
		return;
	}

	// Modal Window Dimensions (Widened to 440px so all columns have generous spacing)
	float winX = 100.0f;
	float winY = 60.0f;
	float winW = 440.0f;
	float winH = 340.0f;

	// Sleek dark glass panel with a crisp 1px cyan border line (Zero yellow texture bleed)
	vec4_t bgColor     = { 0.04f, 0.07f, 0.14f, 0.94f };
	vec4_t borderColor = { 0.00f, 0.65f, 0.95f, 0.85f };

	SCR_FillRect( winX, winY, winW, winH, bgColor );
	SCR_FillRect( winX, winY, winW, 1.0f, borderColor );                 // Top
	SCR_FillRect( winX, winY + winH - 1.0f, winW, 1.0f, borderColor );     // Bottom
	SCR_FillRect( winX, winY, 1.0f, winH, borderColor );                 // Left
	SCR_FillRect( winX + winW - 1.0f, winY, 1.0f, winH, borderColor );     // Right

	// Header background bar
	vec4_t headerBg = { 0.08f, 0.18f, 0.35f, 0.90f };
	SCR_FillRect( winX + 4.0f, winY + 4.0f, winW - 8.0f, 26.0f, headerBg );

	// Title
	vec4_t yellowCol = { 1.0f, 0.85f, 0.20f, 1.0f };
	vec4_t whiteColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	SCR_DrawVirtualString( winX + 120.0f, winY + 9.0f, 7.5f, "^3TOP RANKED DUELISTS", yellowCol );

	// Close Button instruction
	SCR_DrawVirtualString( winX + winW - 45.0f, winY + 9.0f, 6.0f, "^1[ESC]", yellowCol );

	// Column Headers Divider line
	vec4_t divColor = { 0.20f, 0.65f, 1.00f, 0.70f };
	float colY = winY + 36.0f;
	SCR_FillRect( winX + 8.0f, colY + 16.0f, winW - 16.0f, 1.0f, divColor );

	// Column Headers: # | PLAYER NAME | LVL | RANK | FR ELO (Generous column spacing)
	SCR_DrawVirtualString( winX + 16.0f, colY, 6.0f, "^5#", whiteColor );
	SCR_DrawVirtualString( winX + 45.0f, colY, 6.0f, "^5PLAYER NAME", whiteColor );
	SCR_DrawVirtualString( winX + 230.0f, colY, 6.0f, "^5LVL", whiteColor );
	SCR_DrawVirtualString( winX + 275.0f, colY, 6.0f, "^5RANK", whiteColor );
	SCR_DrawVirtualString( winX + 375.0f, colY, 6.0f, "^5FR ELO", whiteColor );

	// Render Rows
	float rowY = colY + 20.0f;
	float rowH = 24.0f;

	if ( g_topLeaderboardCount == 0 ) {
		SCR_DrawVirtualString( winX + 120.0f, rowY + 50.0f, 6.5f, "^7Loading leaderboard data...", whiteColor );
	} else {
		for ( int i = 0; i < g_topLeaderboardCount && i < 10; i++ ) {
			topLeaderboardEntry_t *e = &g_topLeaderboard[i];
			float currentY = rowY + (i * rowH);

			// Alternating row background tint
			if ( i % 2 == 0 ) {
				vec4_t rowBg = { 0.06f, 0.12f, 0.22f, 0.40f };
				SCR_FillRect( winX + 6.0f, currentY - 2.0f, winW - 12.0f, 22.0f, rowBg );
			}

			// Rank Badge Color
			char rankBadge[16];
			if ( e->rank == 1 ) Com_sprintf( rankBadge, sizeof(rankBadge), "^3%2d.", e->rank );      // Gold
			else if ( e->rank == 2 ) Com_sprintf( rankBadge, sizeof(rankBadge), "^7%2d.", e->rank ); // Silver
			else if ( e->rank == 3 ) Com_sprintf( rankBadge, sizeof(rankBadge), "^1%2d.", e->rank ); // Bronze
			else Com_sprintf( rankBadge, sizeof(rankBadge), "^5%2d.", e->rank );

			// Rank #
			SCR_DrawVirtualString( winX + 14.0f, currentY + 2.0f, 6.0f, rankBadge, whiteColor );

			// Player Name (Full names up to 20 characters with generous spacing)
			char nameStr[64];
			Com_sprintf( nameStr, sizeof(nameStr), "^7%.20s", e->displayName );
			SCR_DrawVirtualString( winX + 45.0f, currentY + 2.0f, 6.0f, nameStr, whiteColor );

			// Level Badge
			char lvlStr[16];
			Com_sprintf( lvlStr, sizeof(lvlStr), "^3Lv %d", e->level );
			SCR_DrawVirtualString( winX + 228.0f, currentY + 2.0f, 5.5f, lvlStr, whiteColor );

			// Rank Title (Full rank title up to 16 characters: "Grand Master" fits cleanly!)
			char titleStr[32];
			Com_sprintf( titleStr, sizeof(titleStr), "^3%.16s", e->rankTitle );
			SCR_DrawVirtualString( winX + 275.0f, currentY + 2.0f, 5.5f, titleStr, whiteColor );

			// FR ELO
			char frStr[32];
			Com_sprintf( frStr, sizeof(frStr), "^2%d", e->fr );
			SCR_DrawVirtualString( winX + 375.0f, currentY + 2.0f, 6.0f, frStr, whiteColor );
		}
	}

	// Footer instruction
	SCR_DrawVirtualString( winX + 100.0f, winY + winH - 16.0f, 5.5f, "^7Press ^3F8^7, ^3ESC^7, or type ^3!top^7 to close", whiteColor );
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
			SCR_DrawRPGHUDOverlay();
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
