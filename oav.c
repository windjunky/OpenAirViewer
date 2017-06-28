/****************************************************************************
 *                                                                          *
 * Programm: oav                                                            *
 *                                                                          *
 * Aufgabe : OpenAirViewer                                                  *
 *                                                                          *
 ****************************************************************************/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <richedit.h>
#include <stdio.h>
#include <math.h>
#include "oav.h"

#define PI         3.14159265358979323846
#define RHO       57.29577951308232087685

#define IBOXH		140
#define IBOXS1		100
#define IBOXS2		250
#define IBOXB		IBOXS1+IBOXS2+5

/** Prototypes **************************************************************/

LRESULT WINAPI WndProc ( HWND, UINT, WPARAM, LPARAM );

/** Global variables ********************************************************/

HANDLE ghInstance;
HWND   hApp, hWndTV, hWndLV, hWndSB;
HMENU  hMenu;
DWORD  dwContSel;
DWORD  dwContZ = 0, dwAreaZ, dwPointZ, dwCircleZ, dwStringsIdx; 

float  fLatMin, fLatMax;	// Min Max Werte des gesamten Files
float  fLonMin, fLonMax;
float  fMidLat, fMidLon;	// Mittelpunkte
int    nMidX, nMidY;

float  fDivLat, fDivLon;

RECT   sRectView;

TV_INSERTSTRUCT  sTvins;

struct strContainer
{
	BYTE   bTyp;				// 0 = File / 1 = Area / 2 = Point / 3 = Circle
	DWORD  dwIndex;				// Index nach Typ
	HANDLE hParent;
};
struct strContainer *sCont = NULL;

struct strArea
{
	DWORD  dwClass;				// Index auf Strings
	DWORD  dwName;
	DWORD  dwHigh;
	DWORD  dwLow;
	DWORD  dwElemIdx;			// Index auf Elemente (Container)
	DWORD  dwElemAnz;
	float  fLatMin, fLatMax;	// Min Max Werte der Area
	float  fLonMin, fLonMax;
};
struct strArea *sArea = NULL;

struct strPoint
{
	DWORD  dwArea;				// Index der Area
	float  fLat, fLon;
};
struct strPoint *sPoint = NULL;

struct strCircle
{
	BYTE   bTyp;				// Drehrichtung
	DWORD  dwArea;				// Index der Area
	float  fRad;
	float  fLatM, fLonM;
	float  fLatB, fLonB;
	float  fLatE, fLonE;
};
struct strCircle *sCircle = NULL;

char *pStrings = NULL;


/****************************************************************************
 *                                                                          *
 * Function: LoadRtfResource                                                *
 *           EditStreamCallback                                             *
 *                                                                          *
 * Purpose : RTF support function                                           *
 *                                                                          *
 ****************************************************************************/

static DWORD EditStreamCallback ( DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb )
{
	char    szResIdStr[32];
	HRSRC   hResInfo = NULL;
	static HGLOBAL hResource = NULL;
	static DWORD   dwResSize = 0, dwRead = 0;
	static LPBYTE  pbResData = NULL;

	// avoid loading the resource more than once
	
	if ( dwRead == 0 )
	{
		sprintf ( szResIdStr, "#%d", dwCookie );

		hResInfo  = FindResource ( ghInstance, szResIdStr, RT_RCDATA );

		if ( hResInfo != NULL )
		{
			hResource = LoadResource   ( ghInstance, hResInfo );
			dwResSize = SizeofResource ( ghInstance, hResInfo );
			
			if ( ( hResource != NULL ) && ( dwResSize > 0 ) )
			{
				pbResData = (LPBYTE) LockResource ( hResource );
			}
		}
	}

	if ( pbResData != NULL )
	{
		memset ( pbBuff, 0, cb );

		if ( ( dwResSize - dwRead ) > cb )
		{
			CopyMemory ( pbBuff, pbResData + dwRead, cb );
			
			*pcb = cb;
			
			dwRead += *pcb;
		}
		else
		{
			memcpy ( pbBuff, pbResData + dwRead, dwResSize - dwRead );

			*pcb = dwResSize - dwRead;

			FreeResource ( hResource );

			hResource = NULL;
			pbResData = NULL;
			dwResSize = 0;
			dwRead    = 0;
		}
		return 0;
	}
	return 1;
}
static DWORD LoadRtfResource ( HWND hwndRichEdit, int iResID )
{
	EDITSTREAM es;

	memset ( &es, 0, sizeof ( es ) );
        
	es.pfnCallback = EditStreamCallback;
	es.dwCookie    = iResID;
	es.dwError     = 0;
	
	RichEdit_StreamIn ( hwndRichEdit, SF_RTF, &es );

	return es.dwError;
}


/****************************************************************************
 *                                                                          *
 * Programm: HelpDlgProc                                                    *
 *                                                                          *
 * Aufgabe : Help Dialog ausgeben mit RichText                              *
 *                                                                          *
 ****************************************************************************/

LRESULT CALLBACK HlpDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch ( uMsg )
    {
        case WM_INITDIALOG:
		{
			LoadRtfResource ( GetDlgItem ( hDlg, IDB_RTFI ), IDR_RTFI );

			return FALSE;
		}

		case WM_COMMAND:

			if ( LOWORD ( wParam ) == IDOK || LOWORD ( wParam ) == IDCANCEL )
    		{
				EndDialog ( hDlg, TRUE );

				return TRUE;
			}
    }
    return FALSE;
}


/****************************************************************************
 *                                                                          *
 * Programm: WinMain                                                        *
 *                                                                          *
 * Aufgabe : Fensterklasse registrieren                                     *
 *                                                                          *
 ****************************************************************************/

int PASCAL WinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance,
	                 LPSTR lpCmdLine, int nCmdShow)
{
	char szAppName[]  = "OpenAir Viewer";
	char szAppClass[] = "OpenAirClass";

	WNDCLASS wc;
	MSG      msg;

	ghInstance = hInstance;
    
	InitCommonControls ();

    LoadLibrary ( "riched32.dll" );

	wc.style	     = 0;
	wc.lpfnWndProc	 = (WNDPROC) WndProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra 	 = 0;
	wc.hInstance	 = hInstance;
	wc.hIcon	     = LoadIcon ( hInstance, MAKEINTRESOURCE ( IDI_MAIN ) );
	wc.hCursor	     = 0;
	wc.hbrBackground = (HBRUSH) COLOR_WINDOW;
	wc.lpszMenuName	 = MAKEINTRESOURCE ( IDM_MAIN );
	wc.lpszClassName = szAppClass;

	if ( ! RegisterClass ( &wc ) ) return 0;

	hApp = CreateWindow ( szAppClass, szAppName,
	                      WS_OVERLAPPEDWINDOW,
		                  CW_USEDEFAULT, CW_USEDEFAULT,
		                  CW_USEDEFAULT, CW_USEDEFAULT,
		                  NULL, NULL, ghInstance, NULL);
	
	if ( ! hApp ) return 0;
	
	ShowWindow ( hApp, nCmdShow );
	
	UpdateWindow ( hApp );

	while ( GetMessage ( &msg, NULL, 0, 0 ) )
	{
		TranslateMessage ( &msg );		
		DispatchMessage  ( &msg );
	}
	 return msg.wParam;
}


/****************************************************************************
 *                                                                          *
 * Programm: ReadString                                                    	*
 *                                                                          *
 * Aufgabe : Speichert einen String und gibt den Pointer darauf zurück		*
 *                                                                          *
 ****************************************************************************/

DWORD ReadString ( char *szString )
{
	DWORD dwLen = strlen ( szString ) + 1;

	if ( szString[dwLen-2] == 10 ) szString[--dwLen-1] = 0;

	pStrings = realloc ( pStrings, dwStringsIdx + dwLen );

	if ( pStrings == NULL ) return ( 0 );

	strcpy ( pStrings + dwStringsIdx, szString );

	DWORD dwPos = dwStringsIdx;

	dwStringsIdx += dwLen;

	return ( dwPos );
}


/****************************************************************************
 *                                                                          *
 * Programm: ReadKoor                                                    	*
 *                                                                          *
 * Aufgabe : Liste eine Koordinate											*
 *                                                                          *
 ****************************************************************************/

float ReadKoor ( char *pString )
{
	char *pPtr = pString;
	float fVal = 0.0, fDiv = 1.0;

	BYTE bI;

	for ( bI=0; bI<4; bI++ )
	{
		char *pRet = strtok ( pPtr, " :,\n" );

		if ( pRet == NULL ) return ( 0.0 );

		if ( strlen ( pRet ) == 1 && strchr ( "NnSsWwEeOo", *pRet ) != NULL )
		{
			if ( strchr ( "SsWw", *pRet ) != NULL ) fVal = - fVal;

			return ( fVal );
		}
	
		fVal += atof ( pRet ) / fDiv;
	
		fDiv *= 60.0;
		pPtr = NULL;
	}
	return ( 0.0 );
}


/****************************************************************************
 *                                                                          *
 * Programm: SetMax                                                      	*
 *                                                                          *
 * Aufgabe : Set die Maximalwerte (oder Startwert)							*
 *                                                                          *
 ****************************************************************************/

void SetMax ( float fLat, float fLon, float fRad )
{
	if ( ! sArea[dwAreaZ-1].dwElemAnz )
	{
		sArea[dwAreaZ-1].dwElemIdx = dwContZ;
		sArea[dwAreaZ-1].fLatMin   = fLat - fRad;
		sArea[dwAreaZ-1].fLatMax   = fLat + fRad;
		sArea[dwAreaZ-1].fLonMin   = fLon - fRad;
		sArea[dwAreaZ-1].fLonMax   = fLon + fRad;
	}
	else
	{
		if ( sArea[dwAreaZ-1].fLatMin > ( fLat - fRad ) )
								
			sArea[dwAreaZ-1].fLatMin = fLat - fRad;

		if ( sArea[dwAreaZ-1].fLatMax < ( fLat + fRad ) )

			sArea[dwAreaZ-1].fLatMax = fLat + fRad;

		if ( sArea[dwAreaZ-1].fLonMin > ( fLon - fRad ) )

			sArea[dwAreaZ-1].fLonMin = fLon - fRad;

		if ( sArea[dwAreaZ-1].fLonMax < ( fLon + fRad ) )

			sArea[dwAreaZ-1].fLonMax = fLon + fRad;
	}
	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: Dist                                                       	*
 *                                                                          *
 * Aufgabe : Berechnet eine Distanz											*
 *                                                                          *
 ****************************************************************************/

float Dist ( float fX, float fY )
{
	return ( sqrt ( fX * fX + fY * fY ) );
}


/****************************************************************************
 *                                                                          *
 * Programm: ReadFile                                                    	*
 *                                                                          *
 * Aufgabe : Liest die OpenAirDatei                                    		*
 *                                                                          *
 ****************************************************************************/

void ReadOAFile ( char *szFile )
{
	char   szBuffer[256];
	FILE  *pFile;
	DWORD  dwAktArea;
	BOOL   bNewArea = TRUE, bNewPoint = TRUE;
	BOOL   bCircleDir;
	float  fCircleLon, fCircleLat;

	TreeView_SelectItem ( hWndTV, 0 );
  
	TreeView_DeleteAllItems ( hWndTV );

	if ( ( pFile = fopen ( szFile, "rt" ) ) == NULL )
	{
		MessageBox ( hApp, "OpenAir Datei konnte nicht geöffnet werden", "Datei öffnen", MB_ICONEXCLAMATION );
		
		return;
	}

	dwContZ = dwAreaZ = dwPointZ = dwCircleZ, dwStringsIdx = 0;

	if ( sCont    != NULL ) free ( sCont );
	if ( sArea    != NULL ) free ( sArea );
	if ( sPoint   != NULL ) free ( sPoint );
	if ( sCircle  != NULL ) free ( sCircle );
	if ( pStrings != NULL ) free ( pStrings );

	sArea    = NULL;
	sPoint   = NULL;
	sCircle  = NULL;
	pStrings = NULL;
	
	ReadString ( "<NULL>" );

	dwContZ = dwAreaZ = dwPointZ = dwCircleZ = 0; 

	sCont = malloc ( sizeof ( struct strContainer ) );

	memset ( &sTvins, 0, sizeof ( sTvins ) );

	sTvins.item.mask           = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	sTvins.item.cchTextMax     = MAX_PATH ;
	sTvins.item.pszText        = strrchr ( szFile, '\\' ) + 1;
	sTvins.item.iImage         = 1;

	sCont[0].bTyp    = 0;
	sCont[0].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

	dwContZ = 1;

	while ( ! feof ( pFile ) )
	{
		if ( ! fgets ( szBuffer, 255, pFile ) ) continue;

		switch ( szBuffer[0] )
		{
			case 'A':							// Area ...
			{
				if ( bNewArea )
				{
					sArea = realloc ( sArea, ( dwAreaZ + 1 ) * sizeof ( struct strArea ) );

					memset ( sArea + dwAreaZ, 0, sizeof ( struct strArea ) );

					bNewArea   = FALSE;
					bNewPoint  = TRUE;

					bCircleDir = FALSE;
				}

				switch ( szBuffer[1] )
				{
					case 'C':							// AC Class
					{
						sArea[dwAreaZ].dwClass = ReadString ( szBuffer + 3 );

						break;
					}

					case 'N':							// AN Name
					{
						sArea[dwAreaZ].dwName = ReadString ( szBuffer + 3 );

						break;
					}

					case 'H':							// AH High
					{
						sArea[dwAreaZ].dwHigh = ReadString ( szBuffer + 3 );

						break;
					}

					case 'L':							// AL Low
					{
						sArea[dwAreaZ].dwLow = ReadString ( szBuffer + 3 );

						break;
					}
				}
				break;

			}

			case 'D':							// Points and Circles
			{
				if ( ! bNewArea )
				{
					sTvins.hParent			   = sCont[0].hParent;
					sTvins.item.lParam         = dwContZ;		
					sTvins.item.pszText        = pStrings + sArea[dwAreaZ].dwName;
					sTvins.item.iImage         = 3;
					sTvins.item.iSelectedImage = 2;

					dwAktArea = dwContZ;
						
					dwContZ ++;

					sCont = realloc ( sCont, ( dwContZ ) * sizeof ( struct strContainer ) );

					sCont[dwAktArea].bTyp    = 1;
					sCont[dwAktArea].dwIndex = dwAreaZ;
					sCont[dwAktArea].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

					dwAreaZ++;

					bNewArea = TRUE;
				}

				switch ( szBuffer[1] )
				{
					case 'A':							// DA ArcCircle
					{
						float fWinkel;

						sCircle = realloc ( sCircle, ( dwCircleZ + 1 ) * sizeof ( struct strCircle ) );

						sCircle[dwCircleZ].bTyp   = bCircleDir;
						sCircle[dwCircleZ].dwArea = dwAktArea;
						sCircle[dwCircleZ].fRad   = atof ( strtok ( szBuffer + 3, " :,\n" ) ) / 60.0;

						fWinkel = atof ( strtok ( szBuffer + 3, " :,\n" ) ) / RHO;

						sCircle[dwCircleZ].fLatB = sin ( fWinkel ) * sCircle[dwCircleZ].fRad + fCircleLat;
						sCircle[dwCircleZ].fLonB = cos ( fWinkel ) * sCircle[dwCircleZ].fRad + fCircleLon;

						fWinkel = atof ( strtok ( szBuffer + 3, " :,\n" ) ) / RHO;

						sCircle[dwCircleZ].fLatE = sin ( fWinkel ) * sCircle[dwCircleZ].fRad + fCircleLat;
						sCircle[dwCircleZ].fLonE = cos ( fWinkel ) * sCircle[dwCircleZ].fRad + fCircleLon;
						sCircle[dwCircleZ].fLatM = fCircleLat;
						sCircle[dwCircleZ].fLonM = fCircleLon;

						char szBuffer[18];

						sprintf ( szBuffer, "%03lu Kreis", sArea[dwAreaZ-1].dwElemAnz + 1 );

						sTvins.hParent		       = sCont[dwAktArea].hParent;
						sTvins.item.lParam         = dwContZ;		
						sTvins.item.pszText        = szBuffer;
						sTvins.item.iImage         = 11;
						sTvins.item.iSelectedImage = 10;

						sCont = realloc ( sCont, ( dwContZ + 1 ) * sizeof ( struct strContainer ) );
						
						sCont[dwContZ].bTyp    = 3;
						sCont[dwContZ].dwIndex = dwCircleZ;
						sCont[dwContZ].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

						SetMax ( sCircle[dwCircleZ].fLatM, sCircle[dwCircleZ].fLonM, sCircle[dwCircleZ].fRad );

						sArea[dwAreaZ-1].dwElemAnz++;
					
						dwContZ ++;
						dwCircleZ++;

						bNewPoint = FALSE;

						break;
					}

					case 'B':							// DB ArcCircle
					{
						sCircle = realloc ( sCircle, ( dwCircleZ + 1 ) * sizeof ( struct strCircle ) );

						sCircle[dwCircleZ].bTyp   = bCircleDir;
						sCircle[dwCircleZ].dwArea = dwAktArea;

						float fCos = cos ( fCircleLat / RHO );

						sCircle[dwCircleZ].fLatB = ReadKoor ( szBuffer + 3 );
						sCircle[dwCircleZ].fLonB = ReadKoor ( NULL );
						sCircle[dwCircleZ].fLatE = ReadKoor ( NULL );
						sCircle[dwCircleZ].fLonE = ReadKoor ( NULL );
						sCircle[dwCircleZ].fRad  = ( Dist (   sCircle[dwCircleZ].fLatB - fCircleLat,
														    ( sCircle[dwCircleZ].fLonB - fCircleLon ) * fCos ) +
													 Dist (   sCircle[dwCircleZ].fLatE - fCircleLat,
														    ( sCircle[dwCircleZ].fLonE - fCircleLon ) * fCos ) ) / 2.0;
						sCircle[dwCircleZ].fLatM = fCircleLat;
						sCircle[dwCircleZ].fLonM = fCircleLon;

						char szBuffer[18];

						sprintf ( szBuffer, "%03lu Kreisbogen", sArea[dwAreaZ-1].dwElemAnz + 1 );

						sTvins.hParent		       = sCont[dwAktArea].hParent;
						sTvins.item.lParam         = dwContZ;		
						sTvins.item.pszText        = szBuffer;
						sTvins.item.iImage         = 9;
						sTvins.item.iSelectedImage = 8;

						sCont = realloc ( sCont, ( dwContZ + 1 ) * sizeof ( struct strContainer ) );
						
						sCont[dwContZ].bTyp    = 3;
						sCont[dwContZ].dwIndex = dwCircleZ;
						sCont[dwContZ].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

						SetMax ( sCircle[dwCircleZ].fLatM, sCircle[dwCircleZ].fLonM, sCircle[dwCircleZ].fRad );

						sArea[dwAreaZ-1].dwElemAnz++;
					
						dwContZ ++;
						dwCircleZ++;

						bNewPoint = FALSE;

						break;
					}

					case 'C':							// DC Full circle
					{
						sCircle = realloc ( sCircle, ( dwCircleZ + 1 ) * sizeof ( struct strCircle ) );

						sCircle[dwCircleZ].dwArea = dwAktArea;
						sCircle[dwCircleZ].fRad   = atof ( szBuffer + 3 ) / 60.0;
						sCircle[dwCircleZ].fLatM  = fCircleLat;
						sCircle[dwCircleZ].fLonM  = fCircleLon;

						char szBuffer[12];

						sprintf ( szBuffer, "%03lu Kreis", sArea[dwAreaZ-1].dwElemAnz + 1 );

						sTvins.hParent		       = sCont[dwAktArea].hParent;
						sTvins.item.lParam         = dwContZ;		
						sTvins.item.pszText        = szBuffer;
						sTvins.item.iImage         = 11;
						sTvins.item.iSelectedImage = 10;

						sCont = realloc ( sCont, ( dwContZ + 1 ) * sizeof ( struct strContainer ) );
						
						sCont[dwContZ].bTyp    = 4;
						sCont[dwContZ].dwIndex = dwCircleZ;
						sCont[dwContZ].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

						SetMax ( sCircle[dwCircleZ].fLatM, sCircle[dwCircleZ].fLonM, sCircle[dwCircleZ].fRad );

						sArea[dwAreaZ-1].dwElemAnz++;
					
						dwContZ ++;
						dwCircleZ++;

						bNewPoint = TRUE;

						break;
					}

					case 'P':							// DP Point
					{
						sPoint = realloc ( sPoint, ( dwPointZ + 1 ) * sizeof ( struct strPoint ) );

						sPoint[dwPointZ].dwArea = dwAktArea;
						sPoint[dwPointZ].fLat   = ReadKoor ( szBuffer + 3 );
						sPoint[dwPointZ].fLon   = ReadKoor ( NULL );

						char szBuffer[12];

						sprintf ( szBuffer, "%03lu Punkt", sArea[dwAreaZ-1].dwElemAnz + 1 );

						sTvins.hParent		= sCont[dwAktArea].hParent;
						sTvins.item.lParam  = dwContZ;		
						sTvins.item.pszText = szBuffer;
			
						if ( bNewPoint )
						{
							sTvins.item.iImage         = 5;
							sTvins.item.iSelectedImage = 4;
						}
						else
						{
							sTvins.item.iImage         = 7;
							sTvins.item.iSelectedImage = 6;
						}

						sCont = realloc ( sCont, ( dwContZ + 1 ) * sizeof ( struct strContainer ) );
						
						sCont[dwContZ].bTyp    = 2;
						sCont[dwContZ].dwIndex = dwPointZ;
						sCont[dwContZ].hParent = TreeView_InsertItem ( hWndTV, &sTvins );

						SetMax ( sPoint[dwPointZ].fLat, sPoint[dwPointZ].fLon, 0 );

						sArea[dwAreaZ-1].dwElemAnz++;

						dwContZ++;					
						dwPointZ++;

						bNewPoint = FALSE;

						break;
					}
				}
			}

			case 'V':							// Circle parameter
			{
				char *pPtr = szBuffer + 1;

				while ( *pPtr == ' ' ) pPtr++;

				if ( pPtr[1] == '=' )
				{
					if ( *pPtr == 'D' || *pPtr == 'd' )
					{
						if ( pPtr[2] == '-' ) 
						{	
							bCircleDir = TRUE;
					    }
						else if ( pPtr[2] == '+' )
					    {
                            bCircleDir = FALSE;
						}
					}
					else if ( *pPtr == 'X' || *pPtr == 'x' )
					{
						fCircleLat = ReadKoor ( pPtr + 2 );
						fCircleLon = ReadKoor ( NULL );
					}
				}
			}
		}
	}
	fclose ( pFile );

	TreeView_SelectItem ( hWndTV, sCont[0].hParent );

	if ( dwAreaZ )
	{
		fLatMin = sArea[0].fLatMin;
		fLatMax = sArea[0].fLatMax;
		fLonMin = sArea[0].fLonMin;
		fLonMax = sArea[0].fLonMax;

		DWORD dwI;

		for ( dwI=1; dwI<dwAreaZ; dwI++ )
		{
			if ( fLatMin > sArea[dwI].fLatMin ) fLatMin = sArea[dwI].fLatMin;
			if ( fLatMax < sArea[dwI].fLatMax ) fLatMax = sArea[dwI].fLatMax;
			if ( fLonMin > sArea[dwI].fLonMin ) fLonMin = sArea[dwI].fLonMin;
			if ( fLonMax < sArea[dwI].fLonMax )	fLonMax = sArea[dwI].fLonMax;
		}
	}

	sprintf ( szBuffer,
			  "%lu Flächen mit %lu Punkten und %lu Kreisen bzw. Kreisbögen",
			  dwAreaZ, dwPointZ, dwCircleZ );

	SendDlgItemMessage ( hApp, IDC_STBA, SB_SETTEXT, 0, (LPARAM) szBuffer );

	SetFocus ( hWndTV );

	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: InsertItem                                                    	*
 *                                                                          *
 * Aufgabe : List Fenster füllen                                       		*
 *                                                                          *
 ****************************************************************************/

void InsertItem ( int nItem, char *szInfo, char *szString )
{
	if ( nItem == 0 ) ListView_DeleteAllItems ( hWndLV );
			
	LV_ITEM lvItem;

	memset ( &lvItem, 0, sizeof ( LV_ITEM ) );

	lvItem.mask = LVIF_TEXT;
			
	lvItem.iItem   = nItem;
	lvItem.pszText = szInfo;

	ListView_InsertItem  ( hWndLV, &lvItem );
	ListView_SetItemText ( hWndLV, nItem, 1, szString );

	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: TextPosOut                                                    	*
 *                                                                          *
 * Aufgabe : Einen Text positioniert ausgeben                          		*
 *                                                                          *
 ****************************************************************************/

void TextPosOut ( HDC hDC, int nX, int nY, int nMidX, int nMidY, DWORD dwE )
{
#define OFFSET 2

	WORD wAlign;

	if ( nX > nMidX )
	{
		wAlign = TA_LEFT;

		nX += OFFSET;
	}
	else
	{
		wAlign = TA_RIGHT;

		nX -= OFFSET;
	}

	if ( nY < nMidY )
	{
		wAlign += TA_BOTTOM;

		nY -= OFFSET;
	}
	else
	{
		wAlign += TA_TOP;

		nY += OFFSET;
	}

	SetTextAlign ( hDC, wAlign );	

	char szBuffer[5];

	sprintf ( szBuffer, " %lu", dwE + 1 );

	TextOut ( hDC, nX, nY, szBuffer, strlen ( szBuffer ) );

	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: ViewArea	                                                  	*
 *                                                                          *
 * Aufgabe : Darstellungsroutine einer Fläche                          		*
 *                                                                          *
 ****************************************************************************/

void ViewArea ( HDC hDC, DWORD dwIndex, DWORD dwElem, BOOL bMark )
{
	DWORD dwE;

	BOOL  bStart = FALSE;
	int	  nXs, nYs;

	for ( dwE=0; dwE<sArea[dwIndex].dwElemAnz; dwE++ )
	{
		DWORD dwIdx = sCont[sArea[dwIndex].dwElemIdx+dwE].dwIndex;

		switch ( sCont[sArea[dwIndex].dwElemIdx+dwE].bTyp )
		{
			case 2:
			{
				int nX = nMidX + ( sPoint[dwIdx].fLon - fMidLon ) / fDivLon;
				int nY = nMidY - ( sPoint[dwIdx].fLat - fMidLat ) / fDivLat;

				if ( ! bStart )
				{
					nXs    = nX;
					nYs    = nY;
					bStart = TRUE;

					MoveToEx ( hDC, nX, nY, NULL );
				}
				else
				{
					LineTo ( hDC, nX, nY );
				}

				if ( bMark )
				{
					if ( ( dwE + 1 ) == dwElem )
					{
						SetTextColor ( hDC, RGB ( 200, 0, 0 ) );
			
						TextPosOut ( hDC, nX, nY, nMidX, nMidY, dwE );

						SetTextAlign ( hDC, TA_CENTER );	

						TextOut ( hDC, nX, nY - 9, "o", 1 );

						SetTextColor ( hDC, RGB ( 0, 0, 200 ) );
					}
					else
					{
						TextPosOut ( hDC, nX, nY, nMidX, nMidY, dwE );
					}
				}
				break;
			}

			case 3:
			{
				int nXm = nMidX + ( sCircle[dwIdx].fLonM - fMidLon ) / fDivLon;
				int nYm = nMidY - ( sCircle[dwIdx].fLatM - fMidLat ) / fDivLat;
				int nR  = sCircle[dwIdx].fRad / fDivLat;
				int nXb = nMidX + ( sCircle[dwIdx].fLonB - fMidLon ) / fDivLon;
				int nYb = nMidY - ( sCircle[dwIdx].fLatB - fMidLat ) / fDivLat;
				int nXe = nMidX + ( sCircle[dwIdx].fLonE - fMidLon ) / fDivLon;
				int nYe = nMidY - ( sCircle[dwIdx].fLatE - fMidLat ) / fDivLat;

				if ( sCircle[dwIdx].bTyp )
				{
					SetArcDirection ( hDC, AD_COUNTERCLOCKWISE );
				}
				else
				{
					SetArcDirection ( hDC, AD_CLOCKWISE );
				}


				if ( ! bStart )
				{
					nXs    = nXb;
					nYs    = nYb;
					bStart = TRUE;

					MoveToEx ( hDC, nXb, nYb, NULL );
				}

				ArcTo ( hDC, nXm - nR, nYm - nR, nXm + nR, nYm + nR, nXb, nYb, nXe, nYe );

				if ( bMark )
				{
					if ( ( dwE + 1 ) == dwElem )
					{
						SetTextColor ( hDC, RGB ( 200, 0, 0 ) );
			
						TextPosOut ( hDC, nXe, nYe, nMidX, nMidY, dwE );

						SetTextAlign ( hDC, TA_CENTER );	

						TextOut ( hDC, nXe, nYe - 9, "o", 1 );

						SetTextColor ( hDC, RGB ( 0, 0, 200 ) );
					}
					else
					{
						TextPosOut ( hDC, nXe, nYe, nMidX, nMidY, dwE );
					}
				}
				break;
			}

			case 4:
			{
				int nX = nMidX + ( sCircle[dwIdx].fLonM - fMidLon ) / fDivLon;
				int nY = nMidY - ( sCircle[dwIdx].fLatM - fMidLat ) / fDivLat;
				int nR = sCircle[dwIdx].fRad / fDivLat;

				Ellipse ( hDC, nX - nR, nY - nR, nX + nR, nY + nR );

				if ( bMark )
				{
					if ( ( dwE + 1 ) == dwElem )
					{
						SetTextColor ( hDC, RGB ( 250, 0, 0 ) );
			
						TextPosOut ( hDC, nX, nY, nMidX, nMidY, dwE );

						SetTextAlign ( hDC, TA_CENTER );	

						TextOut ( hDC, nX, nY - 9, "o", 1 );

						SetTextColor ( hDC, RGB ( 0, 0, 200 ) );
					}
					else
					{
						TextPosOut ( hDC, nX, nY, nMidX, nMidY, dwE );
					}
				}
				break;
			}
		}
	}
	if ( bStart ) LineTo ( hDC, nXs, nYs );

	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: View	                                                    	*
 *                                                                          *
 * Aufgabe : Die Darstellungsroutine                                   		*
 *                                                                          *
 ****************************************************************************/

void View ( HWND hWnd )
{
	PAINTSTRUCT	pS;
	HDC         hDC;

	hDC = BeginPaint ( hWnd, (LPPAINTSTRUCT) &pS );

	HPEN hPen = CreatePen ( PS_SOLID, 2, RGB ( 150, 150, 150 ) );

	SelectObject ( hDC, hPen );
	SelectObject ( hDC, GetStockObject( WHITE_BRUSH ) );

	Rectangle ( hDC, sRectView.left + 1, sRectView.top + 1, sRectView.right - 2, sRectView.bottom - 2 ); 

	DeleteObject( hPen );

	if ( ! dwContZ )
	{
		EndPaint ( hWnd, (LPPAINTSTRUCT) &pS );

		return;
	}

	hPen = CreatePen ( PS_SOLID, 2, RGB ( 100, 100, 100 ) );

	SelectObject ( hDC, hPen );

	SetROP2 ( hDC, R2_MASKPEN );

	int nDelX = ( sRectView.right  - sRectView.left );
	int nDelY = ( sRectView.bottom - sRectView.top  ); 

	nMidX = sRectView.left + nDelX / 2;
	nMidY = sRectView.top  + nDelY / 2;

	SetTextColor ( hDC, RGB ( 0, 0, 200 ) );
				
	SetBkMode ( hDC, TRANSPARENT );

	switch ( sCont[dwContSel].bTyp )
	{
		case 0:
		{
			float fDelLat = fLatMax - fLatMin;
			float fDelLon = fLonMax - fLonMin;

			fMidLat = fLatMin + fDelLat / 2.0;
			fMidLon = fLonMin + fDelLon / 2.0;

			float fKorr = cos ( fMidLat / RHO );

			fDivLat = max ( fDelLon * fKorr / nDelX, fDelLat / nDelY ) * 1.5;
			fDivLon = fDivLat / fKorr;

			DWORD dwI;

			for ( dwI=0; dwI<dwAreaZ; dwI++ ) ViewArea ( hDC, dwI, 0, FALSE );

			break;
		}

		case 1:
		{
			DWORD dwIndex = sCont[dwContSel].dwIndex;

			float fDelLat = sArea[dwIndex].fLatMax - sArea[dwIndex].fLatMin;
			float fDelLon = sArea[dwIndex].fLonMax - sArea[dwIndex].fLonMin;

			fMidLat = sArea[dwIndex].fLatMin + fDelLat / 2.0;
			fMidLon = sArea[dwIndex].fLonMin + fDelLon / 2.0;

			float fKorr = cos ( fMidLat / RHO );

			fDivLat = max ( fDelLon * fKorr / nDelX, fDelLat / nDelY ) * 1.5;
			fDivLon = fDivLat / fKorr;

			ViewArea ( hDC, dwIndex, 0, TRUE );

			break;
		}

		case 2:
		{
			DWORD dwElem  = sCont[dwContSel].dwIndex;
			DWORD dwCont  = sPoint[dwElem].dwArea;
			DWORD dwIndex = sCont[dwCont].dwIndex;
			
			float fDelLat = sArea[dwIndex].fLatMax - sArea[dwIndex].fLatMin;
			float fDelLon = sArea[dwIndex].fLonMax - sArea[dwIndex].fLonMin;

			fMidLat = sArea[dwIndex].fLatMin + fDelLat / 2.0;
			fMidLon = sArea[dwIndex].fLonMin + fDelLon / 2.0;

			float fKorr = cos ( fMidLat / RHO );

			fDivLat = max ( fDelLon * fKorr / nDelX, fDelLat / nDelY ) * 1.5;
			fDivLon = fDivLat / fKorr;

			ViewArea ( hDC, dwIndex, dwContSel - dwCont, TRUE );

			break;
		}

		case 3:
		case 4:
		{
			DWORD dwElem  = sCont[dwContSel].dwIndex;
			DWORD dwCont  = sCircle[dwElem].dwArea;
			DWORD dwIndex = sCont[dwCont].dwIndex;
			
			float fDelLat = sArea[dwIndex].fLatMax - sArea[dwIndex].fLatMin;
			float fDelLon = sArea[dwIndex].fLonMax - sArea[dwIndex].fLonMin;

			fMidLat = sArea[dwIndex].fLatMin + fDelLat / 2.0;
			fMidLon = sArea[dwIndex].fLonMin + fDelLon / 2.0;

			float fKorr = cos ( fMidLat / RHO );

			fDivLat = max ( fDelLon * fKorr / nDelX, fDelLat / nDelY ) * 1.5;
			fDivLon = fDivLat / fKorr;

			ViewArea ( hDC, dwIndex, dwContSel - dwCont, TRUE );

			break;
		}
	}

	DeleteObject( hPen );

	EndPaint ( hWnd, (LPPAINTSTRUCT) &pS );

	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: ShowInfo                                                    	*
 *                                                                          *
 * Aufgabe : Zeigt die Infos eines Contents                            		*
 *                                                                          *
 ****************************************************************************/

void ShowInfo ( BYTE bTyp, DWORD dwIndex )
{
	char szBuffer[30];

	switch ( bTyp )
	{
		case 0:
		{
			sprintf ( szBuffer, "%lu", dwAreaZ );

			InsertItem ( 0, "Flugzonen", szBuffer );

			sprintf ( szBuffer, "%lu", dwPointZ );

			InsertItem ( 1, "Gesamt Punkte", szBuffer );

			sprintf ( szBuffer, "%lu", dwCircleZ );

			InsertItem ( 2, "Gesamt Kreise", szBuffer );

			return;
		}

		case 1:
		{
			InsertItem ( 0, "Name", pStrings + sArea[dwIndex].dwName );

			BYTE bIdx = 1;

			if ( sArea[dwIndex].dwClass )
							
				InsertItem ( bIdx++, "Klasse", pStrings + sArea[dwIndex].dwClass );

			if ( sArea[dwIndex].dwHigh )
							
				InsertItem ( bIdx++, "Obergrenze",   pStrings + sArea[dwIndex].dwHigh );

			if ( sArea[dwIndex].dwLow )
							
				InsertItem ( bIdx++, "Untergrenze",  pStrings + sArea[dwIndex].dwLow );

			sprintf ( szBuffer, "%lu", sArea[dwIndex].dwElemAnz );

			InsertItem ( bIdx++, "Elemente", szBuffer );

			return;
		}

		case 2:
		{
			InsertItem ( 0, "Typ", "Punkt" );

			sprintf ( szBuffer, "%7.4f", sPoint[dwIndex].fLat );

			InsertItem ( 1, "Latitude", szBuffer );

			sprintf ( szBuffer, "%7.4f", sPoint[dwIndex].fLon );

			InsertItem ( 2, "Longitude", szBuffer );

			return;
		}

		case 3:
		{
			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fRad * 60.0 );

			if ( sCircle[dwIndex].bTyp )

				strcat ( szBuffer, " links" );

			else

				strcat ( szBuffer, " rechts" );

			strcat ( szBuffer, "drehend" );

			InsertItem ( 0, "Radius", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLatM );

			InsertItem ( 1, "Mittelpunkt-Lat", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLonM );

			InsertItem ( 2, "Mittelpunkt-Lon", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLatB );

			InsertItem ( 3, "Startpunkt-Lat", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLonB );

			InsertItem ( 4, "Startpunkt-Lon", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLatE );

			InsertItem ( 5, "Endpunkt-Lat", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLonE );

			InsertItem ( 6, "Endpunkt-Lon", szBuffer );

			return;
		}

		case 4:
		{
			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fRad * 60.0 );

			InsertItem ( 0, "Radius", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLatM );

			InsertItem ( 1, "Latitude", szBuffer );

			sprintf ( szBuffer, "%7.4f", sCircle[dwIndex].fLonM );

			InsertItem ( 2, "Longitude", szBuffer );

			return;
		}
	}
	return;
}


/****************************************************************************
 *                                                                          *
 * Programm: WndProc                                                    	*
 *                                                                          *
 * Aufgabe : Das MainFenster                                          		*
 *                                                                          *
 ****************************************************************************/

LRESULT WINAPI WndProc ( HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam )
{
	LV_COLUMN lvc;

	switch ( wMsg )
	{
		case WM_COMMAND:
		
			switch ( wParam )
			{
				case IDM_OPEN:
				{
					OPENFILENAME ofn;
					char         szFile[MAX_PATH] = { 0 };

					memset ( &ofn, 0, sizeof ( ofn ) );

					ofn.lStructSize = sizeof ( ofn );
					ofn.hwndOwner   = hWnd;
					ofn.hInstance   = ghInstance;
					ofn.lpstrFilter = "OpenAir Datei (*.txt)\0*.txt\0Flytec OpenAir Datei (*.faf)\0*.faf\0";
					ofn.lpstrFile   = szFile;
					ofn.nMaxFile    = MAX_PATH;
					ofn.lpstrTitle  = "OpenAir Datei öffnen";
					ofn.Flags       = OFN_EXPLORER;
					ofn.lpstrDefExt = "txt";

					if ( GetOpenFileName ( &ofn ) )	ReadOAFile ( szFile );

					return TRUE;
				}

        		case IDM_HELP:
				{
					DialogBox ( ghInstance, MAKEINTRESOURCE ( IDD_HELP ), hWnd, (DLGPROC) HlpDlgProc );

					return TRUE;
				}

				case IDM_ENDE:
                case IDOK:
				
					PostMessage ( hWnd, WM_CLOSE, 0, 0L );
				
				return ( 0 );
			}
			break;

		case WM_SIZE:
	
			MoveWindow ( hWndTV, 0, 0, IBOXB - 1, HIWORD ( lParam ) - IBOXH - 20, TRUE );
			MoveWindow ( hWndLV, 0, HIWORD ( lParam ) - IBOXH - 20, IBOXB - 1, IBOXH - 1, TRUE );
			MoveWindow ( hWndSB, 0, HIWORD ( lParam ) - 20, LOWORD ( lParam ), HIWORD ( lParam ), TRUE );

			GetClientRect ( hWnd, &sRectView );

			sRectView.bottom -= 20;
			sRectView.left   += IBOXB;

			InvalidateRect ( hWnd, &sRectView, TRUE );

			return 0;

		case WM_PAINT:
		{
			View ( hWnd );

			return 0;
		}

		 case WM_DROPFILES:
		 {
			char szFile[MAX_PATH];

			DragQueryFile ( (HDROP) wParam, 0, szFile, MAX_PATH );

			DragFinish ( (HDROP) wParam );                            // Fertig

			ReadOAFile ( szFile );
	 
			return 0;
		}

		case WM_NOTIFY:
		{
			LPNMHDR lpNmh = (LPNMHDR) lParam; 

			if ( lpNmh->hwndFrom == hWndTV && lpNmh->code == TVN_SELCHANGED )
			{
				HTREEITEM hItem = TreeView_GetSelection ( hWndTV );
				TV_ITEM   tvItem;

				ListView_DeleteAllItems ( hWndLV );
			
				memset ( &tvItem, 0, sizeof ( TV_ITEM ) );
			
				tvItem.hItem = hItem;
				tvItem.mask  = TVIF_PARAM;
			
				TreeView_GetItem ( hWndTV, &tvItem );
 			
				dwContSel = tvItem.lParam;

				ShowInfo ( sCont[dwContSel].bTyp, sCont[dwContSel].dwIndex );

				InvalidateRect ( hWnd, &sRectView, TRUE );
			}
			return 0;
		}


		case WM_CREATE:
			
			InitCommonControls();
 
			DragAcceptFiles ( hWnd, TRUE );
		
			hWndTV = CreateWindowEx ( WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL,
									  WS_CHILD | WS_VISIBLE | WS_TABSTOP |
									  TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
									  0, 0, 0, 0, hWnd, 0, ghInstance, NULL );

			// TreeImages setzen

			HIMAGELIST hImages = ImageList_LoadBitmap ( ghInstance,
														MAKEINTRESOURCE ( IDC_TREE ), 	
														16,	2, CLR_DEFAULT	);	
			if ( hImages )

				TreeView_SetImageList ( hWndTV, hImages, TVSIL_NORMAL );

			hWndLV = CreateWindowEx ( WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL,
									  WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT,
									  0, 0, 0, 0, hWnd, 0, ghInstance, NULL);

			ZeroMemory ( &lvc, sizeof ( lvc ) );
		
			lvc.mask    = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
			lvc.cx      = IBOXS1;
			lvc.pszText = "Info";
			
			ListView_InsertColumn ( hWndLV, 0, &lvc );

			lvc.cx      = IBOXS2;
			lvc.pszText = "Inhalt";
			
			ListView_InsertColumn ( hWndLV, 1, &lvc );

			hWndSB = CreateStatusWindow ( WS_CHILD | WS_VISIBLE | WS_BORDER | SBARS_SIZEGRIP,
										  "OK", hWnd, IDC_STBA );

			return(0);
	
		case WM_DESTROY:

			DragAcceptFiles ( hWnd, FALSE );
		
			PostQuitMessage ( 0 );
		
			return ( 0 );
   
		 default:
			
			return DefWindowProc ( hWnd, wMsg, wParam, lParam );
    }
	return 0;
}
