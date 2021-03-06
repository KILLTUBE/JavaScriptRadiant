/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "stdafx.h"
#include "Radiant.h"
#include "XYWnd.h"
#include "CamWnd.h"
#include "qe3.h"
#include "splines/splines.h"
#include "../duktape/duktapestuff.h"

void DrawPathLines();
void Select_ShiftTexture(int x, int y);
void Select_RotateTexture(int amt);
void Select_ScaleTexture(int x, int y);

int g_nAngleSpeed = 300;
int g_nMoveSpeed = 400;
brush_t* g_pSplitList = NULL;

IMPLEMENT_DYNCREATE(CCamWnd, CWnd);

camera_t *camera = NULL;

CCamWnd::CCamWnd() {
	m_pXYFriend = NULL;
	m_nNumTransBrushes = 0;
	memset(&m_Camera, 0, sizeof(camera_t));
	camera = &m_Camera;
	m_pSide_select = NULL;
	m_bClipMode = false;
	Cam_Init();
}

CCamWnd::~CCamWnd() {}


LRESULT CCamWnd::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
	
	if (uMsg == WM_KEYDOWN) {
		switch (wParam) {
			case 'W':
			case 'A':
			case 'S':
			case 'D':
			case 'Q':
			case 'E':
				return CWnd::WindowProc(uMsg, wParam, lParam);
		}
		g_pParentWnd->OnKeyDown(wParam, 0, 0);
	}

	//switch (uMsg) {
	//    case WM_MOUSEWHEEL:
	//		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
	//			g_pParentWnd->OnViewZoomin();
	//		} else {
	//			g_pParentWnd->OnViewZoomout();
	//		}
	//}

	return CWnd::WindowProc(uMsg, wParam, lParam);
}

BEGIN_MESSAGE_MAP(CCamWnd, CWnd)
	ON_WM_KEYDOWN()
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_NCCALCSIZE()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_KEYUP()
END_MESSAGE_MAP()

LONG WINAPI CamWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		RECT	rect;
		GetClientRect(hWnd, &rect);
		switch (uMsg) {
		case WM_KILLFOCUS:
		case WM_SETFOCUS:
			//SendMessage( hWnd, WM_NCACTIVATE, uMsg == WM_SETFOCUS, 0 );
			return 0;
		case WM_NCCALCSIZE:// don't let windows copy pixels
			DefWindowProc (hWnd, uMsg, wParam, lParam);
			return WVR_REDRAW;
		}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

BOOL CCamWnd::PreCreateWindow(CREATESTRUCT& cs) {
	WNDCLASS wc;
	HINSTANCE hInstance = AfxGetInstanceHandle();
	if (::GetClassInfo(hInstance, CAMERA_WINDOW_CLASS, &wc) == FALSE)
	{
		// Register a new class
		memset (&wc, 0, sizeof(wc));
		wc.style				 = CS_NOCLOSE | CS_OWNDC;
		wc.lpszClassName = CAMERA_WINDOW_CLASS;
		wc.hCursor			 = LoadCursor (NULL,IDC_ARROW);
		wc.lpfnWndProc	 = CamWndProc;
		if (AfxRegisterClass(&wc) == FALSE)
			Error ("CCamWnd RegisterClass: failed");
	}

	cs.lpszClass = CAMERA_WINDOW_CLASS;
	cs.lpszName = "CAM";
	if (cs.style != QE3_CHILDSTYLE)
		cs.style = QE3_SPLITTER_STYLE;

	BOOL bResult = CWnd::PreCreateWindow(cs);

	// See if the class already exists and if not then we need
	// to register our new window class.
	return bResult;
	
}

void CCamWnd::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	bool bPaint = true;
	if (!qwglMakeCurrent( dc.m_hDC, g_qeglobals.d_hglrcBase ))
	{
		Sys_Printf("ERROR: wglMakeCurrent failed..\n ");
		Sys_Printf("Please restart Q3Radiant if the camera view is not working\n");
	}
	else
	{
		QE_CheckOpenGLForErrors();
		g_pSplitList = NULL;
		if (g_bClipMode)
		{
			if (g_Clip1.Set() && g_Clip2.Set())
			{
				g_pSplitList = ( (g_pParentWnd->ActiveXY()->GetViewType() == XZ) ? !g_bSwitch : g_bSwitch) ? &g_brBackSplits : &g_brFrontSplits;
			}
		}
		Cam_Draw ();
		QE_CheckOpenGLForErrors();
		qwglSwapBuffers(dc.m_hDC);
	}
}

void CCamWnd::SetXYFriend(CXYWnd * pWnd) {
	m_pXYFriend = pWnd;
}

void CCamWnd::OnDestroy() {
	QEW_StopGL(GetSafeHwnd(), g_qeglobals.d_hglrcBase, g_qeglobals.d_hdcBase );
	CWnd::OnDestroy();
}

void CCamWnd::OnMouseMove(UINT nFlags, CPoint point) {
	CRect r;
	GetClientRect(r);
	if (GetCapture() == this && (GetKeyState(VK_MENU) & 0x8000) && !((GetKeyState(VK_SHIFT) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000))) {
		if (GetKeyState(VK_CONTROL) & 0x8000)
			Select_RotateTexture(point.y - m_ptLastCursor.y);
		else
		if (GetKeyState(VK_SHIFT) & 0x8000)
			Select_ScaleTexture(point.x - m_ptLastCursor.x, m_ptLastCursor.y - point.y);
		else
			Select_ShiftTexture(point.x - m_ptLastCursor.x, m_ptLastCursor.y - point.y);
	} else {
		Cam_MouseMoved(point.x, r.bottom - 1 - point.y, nFlags);
	}
	m_ptLastCursor = point;
}

void CCamWnd::OnLButtonDown(UINT nFlags, CPoint point) {
	SetFocus();
	m_ptLastCursor = point;
	OriginalMouseDown(nFlags, point);
}

void CCamWnd::OnLButtonUp(UINT nFlags, CPoint point) {
	OriginalMouseUp(nFlags, point);
}

void CCamWnd::OnMButtonDown(UINT nFlags, CPoint point) {
	SetFocus();
	OriginalMouseDown(nFlags, point);
}

void CCamWnd::OnMButtonUp(UINT nFlags, CPoint point) {
	OriginalMouseUp(nFlags, point);
}

void CCamWnd::OnRButtonDown(UINT nFlags, CPoint point) {
	SetFocus();
	//OriginalMouseDown(nFlags, point);
	js_call(ctx, "freefly_start", "");
}

void CCamWnd::OnRButtonUp(UINT nFlags, CPoint point) {
	//OriginalMouseUp(nFlags, point);
	js_call(ctx, "freefly_stop", "");
}

int CCamWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	g_qeglobals.d_hdcBase = GetDC()->m_hDC;
	QEW_SetupPixelFormat(g_qeglobals.d_hdcBase, true);
	if ((g_qeglobals.d_hglrcBase = qwglCreateContext(g_qeglobals.d_hdcBase)) == 0)
		Error("wglCreateContext failed");
	if (!qwglMakeCurrent(g_qeglobals.d_hdcBase, g_qeglobals.d_hglrcBase))
		Error ("wglMakeCurrent failed");
	HFONT hfont = ::CreateFont(
		12,	// logical height of font 
		 6,	// logical average character width 
		 0,	// angle of escapement 
		 0,	// base-line orientation angle 
		 0,	// font weight 
		 0,	// italic attribute flag 
		 0,	// underline attribute flag 
		 0,	// strikeout attribute flag 
		 0,	// character set identifier 
		 0,	// output precision 
		 0,	// clipping precision 
		 0,	// output quality 
		 0,	// pitch and family 
		"system font" // pointer to typeface name string 
	);
	if (!hfont)
		Error( "couldn't create font" );
	::SelectObject(g_qeglobals.d_hdcBase, hfont);
	if ((g_qeglobals.d_font_list = glGenLists (256)) == 0)
		Error( "couldn't create font dlists" );
	// create the bitmap display lists
	// we're making images of glyphs 0 thru 255
	if (g_PrefsDlg.m_bBuggyICD) {
		if ( !qwglUseFontBitmaps (g_qeglobals.d_hdcBase, 1, 255, g_qeglobals.d_font_list-1) )
			Error( "wglUseFontBitmaps faileD" );
	} else {
		if ( !qwglUseFontBitmaps (g_qeglobals.d_hdcBase, 1, 255, g_qeglobals.d_font_list) )
			Error( "wglUseFontBitmaps faileD" );
	}
	// indicate start of glyph display lists
	glListBase (g_qeglobals.d_font_list);
	// report OpenGL information
	Sys_Printf ("GL_VENDOR: %s\n", glGetString (GL_VENDOR));
	Sys_Printf ("GL_RENDERER: %s\n", glGetString (GL_RENDERER));
	Sys_Printf ("GL_VERSION: %s\n", glGetString (GL_VERSION));
	Sys_Printf ("GL_EXTENSIONS: %s\n", glGetString (GL_EXTENSIONS));
	g_qeglobals.d_hwndCamera = GetSafeHwnd();
	return 0;
}

void CCamWnd::OriginalMouseUp(UINT nFlags, CPoint point) {
	CRect r;
	GetClientRect(r);
	Cam_MouseUp(point.x, r.bottom - 1 - point.y, nFlags);
	if (!(nFlags & (MK_LBUTTON|MK_RBUTTON|MK_MBUTTON)))
		ReleaseCapture ();
}

void CCamWnd::OriginalMouseDown(UINT nFlags, CPoint point) {
	CRect r;
	GetClientRect(r);
	//SetFocus();
	SetCapture();
	//if (!(GetKeyState(VK_MENU) & 0x8000))
	//Sys_Printf("point.x=%d point.y=%d r.bottom-1-point.y=%d\n", point.x, point.y, r.bottom - 1 - point.y);
	Cam_MouseDown(point.x, r.bottom - 1 - point.y, nFlags);
}

void CCamWnd::Cam_Init() {
	//m_Camera.draw_mode = cd_texture;
	m_Camera.timing = false;
	m_Camera.origin[0] = 0;
	m_Camera.origin[1] = 20;
	m_Camera.origin[2] = 46;
	m_Camera.color[0] = 0.3;
	m_Camera.color[1] = 0.3;
	m_Camera.color[2] = 0.3;
}

void CCamWnd::Cam_BuildMatrix() {
	float	xa, ya;
	float	matrix[4][4];
	int		i;
	xa = m_Camera.angles[0]/180*Q_PI;
	ya = m_Camera.angles[1]/180*Q_PI;
	// the movement matrix is kept 2d
	m_Camera.forward[0] = cos(ya);
	m_Camera.forward[1] = sin(ya);
	m_Camera.right[0] = m_Camera.forward[1];
	m_Camera.right[1] = -m_Camera.forward[0];
	glGetFloatv (GL_PROJECTION_MATRIX, &matrix[0][0]);
	for (i=0 ; i<3 ; i++) {
		m_Camera.vright[i] = matrix[i][0];
		m_Camera.vup[i] = matrix[i][1];
		m_Camera.vpn[i] = matrix[i][2];
	}
	VectorNormalize (m_Camera.vright);
	VectorNormalize (m_Camera.vup);
	VectorNormalize (m_Camera.vpn);
}

void CCamWnd::Cam_ChangeFloor(qboolean up) {
	brush_t	*b;
	float	d, bestd, current;
	vec3_t	start, dir;
	start[0] = m_Camera.origin[0];
	start[1] = m_Camera.origin[1];
	start[2] = 8192;
	dir[0] = dir[1] = 0;
	dir[2] = -1;
	current = 8192 - (m_Camera.origin[2] - 48);
	if (up)
		bestd = 0;
	else
		bestd = 16384;
	for (b=active_brushes.next ; b != &active_brushes ; b=b->next) {
		if ( b->pTerrain && !Terrain_Ray( start, dir, b, &d ) )
			continue;
		if ( !b->pTerrain && !Brush_Ray (start, dir, b, &d) )
			continue;
		if (up && d < current && d > bestd)
			bestd = d;
		if (!up && d > current && d < bestd)
			bestd = d;
	}
	if (bestd == 0 || bestd == 16384)
		return;
	m_Camera.origin[2] += current - bestd;
	Sys_UpdateWindows (W_CAMERA|W_Z_OVERLAY);
}

void CCamWnd::Cam_PositionDrag() {
	int	x, y;
	Sys_GetCursorPos (&x, &y);
	if (x != m_ptCursor.x || y != m_ptCursor.y) {
		x -= m_ptCursor.x;
		VectorMA (m_Camera.origin, x, m_Camera.vright, m_Camera.origin);
		y -= m_ptCursor.y;
		m_Camera.origin[2] -= y;
		SetCursorPos(m_ptCursor.x, m_ptCursor.y);
		Sys_UpdateWindows (W_CAMERA | W_XY_OVERLAY);
	}
}

void CCamWnd::Cam_MouseDown(int x, int y, int buttons) {
	vec3_t dir;
	float f, r, u;
	int i;
	//
	// calc ray direction
	//
	u = (float)(y - m_Camera.height/2) / (m_Camera.width/2);
	r = (float)(x - m_Camera.width/2) / (m_Camera.width/2);
	f = 1;
	for (i=0 ; i<3 ; i++)
		dir[i] = m_Camera.vpn[i] * f + m_Camera.vright[i] * r + m_Camera.vup[i] * u;
	VectorNormalize (dir);
	GetCursorPos(&m_ptCursor);
	m_nCambuttonstate = buttons;
	m_ptButton.x = x;
	m_ptButton.y = y;
	// LBUTTON = manipulate selection
	// shift-LBUTTON = select
	// middle button = grab texture
	// ctrl-middle button = set entire brush to texture
	// ctrl-shift-middle button = set single face to texture
	int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if ((buttons == MK_LBUTTON)
						|| (buttons == (MK_LBUTTON | MK_SHIFT))
						|| (buttons == (MK_LBUTTON | MK_CONTROL))
						|| (buttons == (MK_LBUTTON | MK_CONTROL | MK_SHIFT))
						|| (buttons == nMouseButton)
						|| (buttons == (nMouseButton|MK_SHIFT))
						|| (buttons == (nMouseButton|MK_CONTROL))
						|| (buttons == (nMouseButton|MK_SHIFT|MK_CONTROL)))
	{

		if (g_PrefsDlg.m_nMouseButtons == 2 && (buttons == (MK_RBUTTON | MK_SHIFT))) {
			// was rightclick here
		} else {
			// something global needs to track which window is responsible for stuff
			Patch_SetView(W_CAMERA);
			Drag_Begin (x, y, buttons, m_Camera.vright, m_Camera.vup,	m_Camera.origin, dir);
		}
		return;
	}
}

void CCamWnd::Cam_MouseUp (int x, int y, int buttons) {
	m_nCambuttonstate = 0;
	Drag_MouseUp (buttons);
}

void CCamWnd::Cam_MouseMoved (int x, int y, int buttons) {
	// left bottom is [0,0]
	// buttons:
	// left = MK_LBUTTON(1) right=2 middle=16
	//Sys_Printf("x=%d y=%d buttons=%d\n", x, y, buttons);
	m_nCambuttonstate = buttons;
	if (!buttons) {
		if ( ( g_qeglobals.d_select_mode == sel_terrainpoint ) || ( g_qeglobals.d_select_mode == sel_terraintexture ) ) {
			vec3_t		dir;
			float		f, r, u;
			int			i;
			//
			// calc ray direction
			//
			u = (float)(y - m_Camera.height/2) / (m_Camera.width/2);
			r = (float)(x - m_Camera.width/2) / (m_Camera.width/2);
			f = 1;
			for (i=0 ; i<3 ; i++)
				dir[i] = m_Camera.vpn[i] * f + m_Camera.vright[i] * r + m_Camera.vup[i] * u;
			VectorNormalize (dir);
			m_ptButton.x = x;
			m_ptButton.y = y;
			Terrain_SelectPointByRay( m_Camera.origin, dir, buttons );
			Sys_UpdateWindows(W_CAMERA);
		}
		return;
	}
	m_ptButton.x = x;
	m_ptButton.y = y;
	//if (buttons == (MK_RBUTTON|MK_CONTROL) ) {
	//	Cam_PositionDrag ();
	//	Sys_UpdateWindows (W_XY|W_CAMERA|W_Z);
	//	return;
	//}
	GetCursorPos(&m_ptCursor);
	if (buttons & (MK_LBUTTON | MK_MBUTTON) ) {
		Drag_MouseMoved (x, y, buttons);
		Sys_UpdateWindows (W_XY|W_CAMERA|W_Z);
	}
}

void CCamWnd::InitCull() {
	int i;
	VectorSubtract (m_Camera.vpn, m_Camera.vright, m_vCull1);
	VectorAdd (m_Camera.vpn, m_Camera.vright, m_vCull2);
	for (i=0 ; i<3 ; i++) {
		if (m_vCull1[i] > 0)
			m_nCullv1[i] = 3+i;
		else
			m_nCullv1[i] = i;
		if (m_vCull2[i] > 0)
			m_nCullv2[i] = 3+i;
		else
			m_nCullv2[i] = i;
	}
}

qboolean CCamWnd::CullBrush (brush_t *b) {
	int i;
	vec3_t point;
	float d;
	if (g_PrefsDlg.m_bCubicClipping) {
		float fLevel = g_PrefsDlg.m_nCubicScale * 64;
		point[0] = m_Camera.origin[0] - fLevel;
		point[1] = m_Camera.origin[1] - fLevel;
		point[2] = m_Camera.origin[2] - fLevel;
		for (i=0; i<3; i++)
			if (b->mins[i] < point[i] && b->maxs[i] < point[i])
				return true;
		point[0] = m_Camera.origin[0] + fLevel;
		point[1] = m_Camera.origin[1] + fLevel;
		point[2] = m_Camera.origin[2] + fLevel;
		for (i=0; i<3; i++)
			if (b->mins[i] > point[i] && b->maxs[i] > point[i])
				return true;
	}
	for (i=0 ; i<3 ; i++)
		point[i] = b->mins[m_nCullv1[i]] - m_Camera.origin[i];
	d = DotProduct (point, m_vCull1);
	if (d < -1)
		return true;
	for (i=0 ; i<3 ; i++)
		point[i] = b->mins[m_nCullv2[i]] - m_Camera.origin[i];
	d = DotProduct (point, m_vCull2);
	if (d < -1)
		return true;
	return false;
}

#if 0
void CCamWnd::DrawLightRadius(brush_t* pBrush) {
	// if lighting
	int nRadius = Brush_LightRadius(pBrush);
	if (nRadius > 0) {
		Brush_SetLightColor(pBrush);
		glEnable(GL_BLEND);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}
#endif

// m_Camera.origin[0], m_Camera.origin[1], m_Camera.origin[2]
// m_Camera.angles[PITCH], m_Camera.angles[YAW]
void CCamWnd::Cam_Draw() {
	brush_t	*brush;
	face_t	*face;
	float	screenaspect;
	float	yfov;
	double	start, end;
	int		i;
	if (!active_brushes.next)
		return;	// not valid yet
	// clear
	QE_CheckOpenGLForErrors();
	glViewport(0, 0, m_Camera.width, m_Camera.height);
	glScissor(0, 0, m_Camera.width, m_Camera.height);
	glClearColor(
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][0],
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][1],
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][2], 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// set up viewpoint
	vec5_t lightPos;
	if (g_PrefsDlg.m_bGLLighting) {
		glEnable(GL_LIGHTING);
		//glEnable(GL_LIGHT0);
		lightPos[0] = lightPos[1] = lightPos[2] = 3.5;
		lightPos[3] = 1.0;
		glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lightPos);
		//qglLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
		//lightPos[0] = lightPos[1] = lightPos[2] = 3.5;
		//qglLightfv(GL_LIGHT0, GL_AMBIENT, lightPos);
	} else {
		glDisable(GL_LIGHTING);
	}
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	screenaspect = (float)m_Camera.width / m_Camera.height;
	yfov = 2*atan((float)m_Camera.height / m_Camera.width)*180/Q_PI;
	qgluPerspective (yfov,	screenaspect,	2,	8192);
	glRotatef (-90,	1, 0, 0);			// put Z going up
	glRotatef (90,	0, 0, 1);			// put Z going up
	glRotatef (m_Camera.angles[0],	0, 1, 0);
	glRotatef (-m_Camera.angles[1],	0, 0, 1);
	glTranslatef (-m_Camera.origin[0],	-m_Camera.origin[1],	-m_Camera.origin[2]);
	Cam_BuildMatrix();
	//if (m_Camera.draw_mode == cd_light)
	//{
//	if (g_PrefsDlg.m_bGLLighting)
//	{
//		VectorCopy(m_Camera.origin, lightPos);
//		lightPos[3] = 1;
//		qglLightfv(GL_LIGHT0, GL_POSITION, lightPos);
//	}
	//}
	InitCull ();
	// draw stuff
	GLfloat lAmbient[] = {1.0, 1.0, 1.0, 1.0};
	switch (m_Camera.draw_mode) {
		case cd_wire:
			glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_TEXTURE_1D);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glColor3f(1.0, 1.0, 1.0);
			//		glEnable (GL_LINE_SMOOTH);
			break;
		case cd_solid:
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			glShadeModel (GL_FLAT);
			glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
			glDisable(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc (GL_LEQUAL);
			break;
		case cd_texture:
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			glShadeModel (GL_FLAT);
			glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc (GL_LEQUAL);
			break;
		case cd_blend:
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
			glShadeModel (GL_FLAT);
			glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
			glEnable(GL_TEXTURE_2D);
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glDisable(GL_DEPTH_TEST);
			glEnable (GL_BLEND);
			glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
	}
	glMatrixMode(GL_TEXTURE);
	m_nNumTransBrushes = 0;
	for (brush = active_brushes.next ; brush != &active_brushes ; brush=brush->next) {
		//DrawLightRadius(brush);
		if (CullBrush (brush))
			continue;
		if (FilterBrush (brush))
			continue;		
		if ((brush->brush_faces->texdef.flags & (SURF_TRANS33 | SURF_TRANS66)) || (brush->brush_faces->d_texture->bFromShader && brush->brush_faces->d_texture->fTrans != 1.0)) {
			m_TransBrushes [ m_nNumTransBrushes++ ] = brush;
		} else {
			//--			if (brush->patchBrush)
			//--				m_TransBrushes [ m_nNumTransBrushes++ ] = brush;
			//--			else
			Brush_Draw(brush);
		}	
	}
	if (g_PrefsDlg.m_bGLLighting) {
		glDisable (GL_LIGHTING);
	}
	//qglDepthMask ( 0 ); // Don't write to depth buffer
	glEnable ( GL_BLEND );
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for ( i = 0; i < m_nNumTransBrushes; i++ ) 
		Brush_Draw (m_TransBrushes[i]);
	//qglDepthMask ( 1 ); // Ok, write now
	glMatrixMode(GL_PROJECTION);
	//
	// now draw selected brushes
	//
	if (g_PrefsDlg.m_bGLLighting) {
		glEnable (GL_LIGHTING);
	}
	glTranslatef (g_qeglobals.d_select_translate[0], g_qeglobals.d_select_translate[1], g_qeglobals.d_select_translate[2]);
	glMatrixMode(GL_TEXTURE);
	brush_t* pList = (g_bClipMode && g_pSplitList) ? g_pSplitList : &selected_brushes;
	// draw normally
	for (brush = pList->next ; brush != pList ; brush=brush->next) {
		//DrawLightRadius(brush);
		//if (brush->patchBrush && g_qeglobals.d_select_mode == sel_curvepoint)
		//	continue;	
		Brush_Draw(brush);
	}
	// blend on top
	glMatrixMode(GL_PROJECTION);
	glDisable (GL_LIGHTING);
	glColor4f(1.0, 0.0, 0.0, 0.3);
	glEnable (GL_BLEND);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_TEXTURE_2D);
	for (brush = pList->next ; brush != pList ; brush=brush->next) {
		if ( (brush->patchBrush && g_qeglobals.d_select_mode == sel_curvepoint) || 
			(brush->terrainBrush && g_qeglobals.d_select_mode == sel_terrainpoint) )
			continue;
		for (face=brush->brush_faces ; face ; face=face->next)
			Face_Draw( face );
	}
	int nCount = g_ptrSelectedFaces.GetSize();
	if (nCount > 0) {
		for (int i = 0; i < nCount; i++) {
			face_t *selFace = reinterpret_cast<face_t*>(g_ptrSelectedFaces.GetAt(i));
			Face_Draw(selFace);
		}
	}
	// non-zbuffered outline
	glDisable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	glColor3f (1, 1, 1);
	for (brush = pList->next ; brush != pList ; brush=brush->next) {
		if (g_qeglobals.dontDrawSelectedOutlines || (brush->patchBrush && g_qeglobals.d_select_mode == sel_curvepoint) ||
			(brush->terrainBrush && g_qeglobals.d_select_mode == sel_terrainpoint))
			continue;
		for (face=brush->brush_faces ; face ; face=face->next)
			Face_Draw( face );
	}
	// edge / vertex flags
	if (g_qeglobals.d_select_mode == sel_vertex) {
		glPointSize (4);
		glColor3f (0,1,0);
		glBegin (GL_POINTS);
		for (i=0 ; i<g_qeglobals.d_numpoints ; i++)
			glVertex3fv (g_qeglobals.d_points[i]);
		glEnd ();
		glPointSize (1);
	} else if (g_qeglobals.d_select_mode == sel_edge) {
		float	*v1, *v2;
		glPointSize (4);
		glColor3f (0,0,1);
		glBegin (GL_POINTS);
		for (i=0 ; i<g_qeglobals.d_numedges ; i++) {
			v1 = g_qeglobals.d_points[g_qeglobals.d_edges[i].p1];
			v2 = g_qeglobals.d_points[g_qeglobals.d_edges[i].p2];
			glVertex3f ( (v1[0]+v2[0])*0.5,(v1[1]+v2[1])*0.5,(v1[2]+v2[2])*0.5);
		}
		glEnd ();
		glPointSize (1);
	}
	g_splineList->draw(static_cast<qboolean>(g_qeglobals.d_select_mode == sel_addpoint || g_qeglobals.d_select_mode == sel_editpoint));
	if (g_qeglobals.selectObject && (g_qeglobals.d_select_mode == sel_addpoint || g_qeglobals.d_select_mode == sel_editpoint)) {
		g_qeglobals.selectObject->drawSelection();
	}
	//
	// draw pointfile
	//
	glEnable(GL_DEPTH_TEST);
	DrawPathLines ();
	if (g_qeglobals.d_pointfile_display_list) {
		Pointfile_Draw();
		//glCallList (g_qeglobals.d_pointfile_display_list);
	}
	// bind back to the default texture so that we don't have problems
	// elsewhere using/modifying texture maps between contexts
	glBindTexture( GL_TEXTURE_2D, 0 );
	
#if 0
	// area selection hack
	if (g_qeglobals.d_select_mode == sel_area)
	{
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(0.0, 0.0, 1.0, 0.25);
		glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
		glRectfv(g_qeglobals.d_vAreaTL, g_qeglobals.d_vAreaBR);
		glDisable (GL_BLEND);
	}
#endif
	glFinish();
	QE_CheckOpenGLForErrors();
}

void CCamWnd::OnSize(UINT nType, int cx, int cy) {
	CWnd::OnSize(nType, cx, cy);
	CRect rect;
	GetClientRect(rect);
	m_Camera.width = rect.right;
	m_Camera.height = rect.bottom;
	InvalidateRect(NULL, false);
}

void CCamWnd::ReInitGL() {
	qwglMakeCurrent(0,0);
	QEW_SetupPixelFormat(GetDC()->m_hDC, true);
	if (!qwglMakeCurrent(g_qeglobals.d_hdcBase, g_qeglobals.d_hglrcBase))
		Error ("wglMakeCurrent failed");
}

// Timo
// brush primitive texture shifting, using camera view to select translations :
void CCamWnd::ShiftTexture_BrushPrimit(face_t *f, int x, int y) {
	vec3_t texS,texT;
	vec3_t viewX,viewY;
	int XS,XT,YS,YT;
	int outS,outT;
#ifdef _DEBUG
	if (!g_qeglobals.m_bBrushPrimitMode) {
		Sys_Printf("Warning : unexpected call to CCamWnd::ShiftTexture_BrushPrimit with brush primitive mode disbaled\n");
		return;
	}
#endif
	// compute face axis base
	ComputeAxisBase( f->plane.normal, texS, texT );
	// compute camera view vectors
	VectorCopy( m_Camera.vup, viewY );
	VectorCopy( m_Camera.vright, viewX );
	// compute best vectors
	ComputeBest2DVector( viewX, texS, texT, XS, XT );
	ComputeBest2DVector( viewY, texS, texT, YS, YT );
	// check this is not a degenerate case
	if ( ( XS == YS ) && ( XT == YT ) ) {
#ifdef _DEBUG
		Sys_Printf("Warning : degenerate best vectors axis base in CCamWnd::ShiftTexture_BrushPrimit\n");
#endif
		// forget it
		Select_ShiftTexture_BrushPrimit( f, x, y );
		return;
	}
	// compute best fitted translation in face axis base
	outS = XS*x + YS*y;
	outT = XT*x + YT*y;
	// call actual texture shifting code
	Select_ShiftTexture_BrushPrimit( f, outS, outT );
}
