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

#define IBOXH         140 // InfoBox-Höhe
#define IBOXS1        100 // InfoBox-Breite Info
#define IBOXS2        250 // InfoBox-Breite Inhalt
#define IBOXB        IBOXS1+IBOXS2+5  // InfoBox-Breite über alles

#define PEN_SIZE_NORMAL  2
#define PEN_SIZE_SELECT  4

#define OA_TYP_FILE      0
#define OA_TYP_AIRSPACE  1
#define OA_TYP_POINT     2
#define OA_TYP_CIRCLE    3
#define OA_TYP_ARC       4

#define OA_ROT_DIR_RIGHT    0 // rechtsrum
#define OA_ROT_DIR_LEFT     1 // linksrum
#define OA_ROT_DIR_DEFAULT  2 // manchmal ist die Drehrichtung für einen Kreisbogen in einer Datei nicht gesetzt, dann default (also rechtsrum)

#define TV_FILE_ICON        0 // Die Indizes der Icons für den TreeView.
#define TV_AIRSPACE_ICON    2 // Das inaktivierte Icon folgt jeweils auf dem nächsten Index.
#define TV_NEW_POINT_ICON   4
#define TV_POINT_ICON       6
#define TV_ARC_ICON         8
#define TV_CIRCLE_ICON      10

#define SELECT_CIRCLE       0 // kreisförmige Auswahl um einen Mittelpunkt
#define SELECT_RECT         1 // rechteckige Auswahl um einen Mittelpunkt

/** Prototypes ************************************************************* */

LRESULT WINAPI WndProc(HWND, UINT, WPARAM, LPARAM);

/** Global variables ******************************************************* */

HANDLE ghInstance;
HWND hApp, hWndTV, hWndLV, hWndSB;
HMENU hMenu;
DWORD dwTreeViewSel;    // der im selektierten TreeView-Knoten hinterlegte Index für einen Container 
DWORD dwContZ = 0, dwAirspaceZ, dwPointZ, dwCircleZ, dwStringsIdx;
char szFile[MAX_PATH];  // Filename der Open-Air-Datei

float fLatMin, fLatMax; // Min-Max-Werte des gesamten Files geographische Breite (Latitude)
float fLonMin, fLonMax; // Min-Max-Werte des gesamten Files geographische Länge (Longitude)
float fMidLat, fMidLon; // Mittelpunkte geographische Breite (Latitude) / Länge (Longitude), wird vor dem Zeichnen berechnet
int nMidX, nMidY;       // Mittelpunkt in Bildschirmkoordinaten, wird vor dem Zeichnen berechnet

float fDivLat, fDivLon;

float fLatSelCenter = 52.0f, fLonSelCenter = 13.0f, fSelSize = 1.0f;  // Auswahl von Lufträumen mit Zentrum und Ausdehnung 
BYTE bSelType = SELECT_RECT;  // Auswahltyp

RECT sRectView;

struct strContainer {
  BYTE bTyp;      // 0 = File / 1 = Airspace / 2 = Point / 3 = Circle / 4 = Arc
  DWORD dwIndex;  // Index für sAirspace, sPoint, sCircle abhängig vom Typ
  HANDLE hParent; // TreeView-Eintrag, übergeordneter Knoten
};
struct strContainer *sCont = NULL;

struct strAirspace {
  DWORD dwClass;          // Klasse: Index für Textspeicher
  DWORD dwName;           // Name: Index für Textspeicher
  DWORD dwHigh;           // Obergrenze: Index für Textspeicher
  DWORD dwLow;            // Untergrenze: Index für Textspeicher
  DWORD dwElemIdx;        // Startindex Unterlemente (sCont)
  DWORD dwElemAnz;        // Anzahl Unterelemente 
  float fLatMin, fLatMax; // Min-Max geographische Breite des Luftraums (vertikal)
  float fLonMin, fLonMax; // Min-Max geographische Länge des Luftraums (horizontal)
  BOOL bValid;            // nicht anwählbare Lufträume werden mit bValid = FALSE gekennzeichnet
};
struct strAirspace *sAirspaces = NULL;

struct strPoint {
  DWORD dwAirspace;   // Index für sAirspace
  float fLat, fLon;
};
struct strPoint *sPoints = NULL;

struct strCircle {
  BYTE bRotDir;       // Drehrichtung 0 - rechts, 1 - links
  DWORD dwAirspace;   // Index für sAirspace
  float fRad;
  float fLatM, fLonM;
  float fLatB, fLonB;
  float fLatE, fLonE;
};
struct strCircle *sCircles = NULL;

char *pStrings = NULL;

/****************************************************************************
 *                                                                          *
 * Function: LoadRtfResource                                                *
 *           EditStreamCallback                                             *
 *                                                                          *
 * Purpose : RTF support functions                                           *
 *                                                                          *
 ****************************************************************************/
static DWORD EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
  char szResIdStr[32];
  HRSRC hResInfo = NULL;
  static HGLOBAL hResource = NULL;
  static LONG lResSize = 0, lRead = 0;
  static LPBYTE pbResData = NULL;

  // avoid loading the resource more than once
  if (lRead == 0)
  {
    sprintf(szResIdStr, "#%d", dwCookie);

    hResInfo = FindResource(ghInstance, szResIdStr, RT_RCDATA);

    if (hResInfo != NULL)
    {
      hResource = LoadResource(ghInstance, hResInfo);
      lResSize = SizeofResource(ghInstance, hResInfo);

      if ((hResource != NULL) && (lResSize > 0))
      {
        pbResData = (LPBYTE)LockResource(hResource);
      }
    }
  }

  if (pbResData != NULL)
  {
    memset(pbBuff, 0, cb);

    if ((lResSize - lRead) > cb)
    {
      CopyMemory(pbBuff, pbResData + lRead, cb);

      *pcb = cb;

      lRead += *pcb;
    }
    else
    {
      memcpy(pbBuff, pbResData + lRead, lResSize - lRead);

      *pcb = lResSize - lRead;

      FreeResource(hResource);

      hResource = NULL;
      pbResData = NULL;
      lResSize = 0;
      lRead = 0;
    }
    return 0;
  }
  return 1;
}

static DWORD LoadRtfResource(HWND hwndRichEdit, int iResID)
{
  EDITSTREAM es;

  memset(&es, 0, sizeof(es));

  es.pfnCallback = EditStreamCallback;
  es.dwCookie = iResID;
  es.dwError = 0;

  RichEdit_StreamIn(hwndRichEdit, SF_RTF, &es);

  return es.dwError;
}

/****************************************************************************
 *                                                                          *
 * Programm: HelpDlgProc                                                     *
 *                                                                          *
 * Aufgabe : Hilfe Dialog ausgeben mit RichText                             *
 *                                                                          *
 ****************************************************************************/
LRESULT CALLBACK HelpDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      LoadRtfResource(GetDlgItem(hDlg, IDB_RTFI), IDR_RTFI);

      return FALSE;
    }

    case WM_COMMAND:

      if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
      {
        EndDialog(hDlg, TRUE);

        return TRUE;
      }
  }
  return FALSE;
}

/****************************************************************************
 *                                                                          *
 * Programm: SelectAirspaces                                                *
 *                                                                          *
 * Aufgabe:  Auswahl von Lufträumen                                         *
 *                                                                          *
 *           fLatCenter: Breitengrad des Zentrums                           *
 *           fLonCenter: Längengrad des Zentrums                            *
 *           fSize: Größe der Auswahl                                       *
 *                  bei SEL_RECT: fLatCenter +- fSize                       *
 *                                fLonCenter +- fSize                       *
 *                  bei SEL_CIRCLE: Radius um fLatCenter/fLonCenter         *
 *           bType: Auswahltyp (OA_SEL_RECT oder OA_TYPE_CIRCLE)            *
 *                                                                          *
 ****************************************************************************/
void SelectAirspaces(float fLatCenter, float fLonCenter, float fSize, BYTE bType)
{
  DWORD dwA;

  // Startwerte für die neuen Min-Max-Werte aller Lufträume auf Auswahl setzen
  fLatMin = fLatCenter - fSize;
  fLatMax = fLatCenter + fSize;
  fLonMin = fLonCenter - fSize;
  fLonMax = fLonCenter + fSize;

  for (dwA = 0; dwA < dwAirspaceZ; dwA++)
  {
    if (SELECT_CIRCLE == bType)
    {
      // kreisförmige Auswahl  
    }
    else if (SELECT_RECT == bType)
    {
      // Todo: Transformation in den vollständig positiven Bereich ?
      //       Breite (Latitude + 180), Länge (Longitude + 360)        

      // -180° westliche Länge ... 0° (Greenwhich) ... 180° östliche Länge
      // -90°  südliche Breite ... 0° (Äquator)    ... 90°  nördliche Breite

      // rechteckige Auswahl
      //                            
      //  +-------------------------------------+
      //  |                  |                  |
      //  |              (+fSize)               |
      //  |                  |                  |
      //  | --( -fSize)-- Center --( +fSize )-- |
      //  |                  |                  |
      //  |              (-fSize)               |
      //  |                  |                  |   
      //  +-------------------------------------+
      //   
      // vier Fälle
      // 1. Luftraum oberhalb des Auswahlrechtecks
      if (sAirspaces[dwA].fLatMin > (fLatCenter + fSize))
      {
        sAirspaces[dwA].bValid = FALSE;
      }
      // 2. Luftraum unterhalb des Auswahlrechtecks
      else if (sAirspaces[dwA].fLatMax < (fLatCenter - fSize))
      {
        sAirspaces[dwA].bValid = FALSE;
      }
      // 3. Luftraum links des Auswahlrechtecks
      else if (sAirspaces[dwA].fLonMax < (fLonCenter - fSize))
      {
        sAirspaces[dwA].bValid = FALSE;
      }
      // 4. Luftraum rechts des Auswahlrechtecks
      else if (sAirspaces[dwA].fLonMin > (fLonCenter + fSize))
      {
        sAirspaces[dwA].bValid = FALSE;
      }
      // alle anderen Lufträume liegen ganz oder teilweise 
      // im Auswahlrechteck 
      else
      {
        sAirspaces[dwA].bValid = TRUE;

        // Grenzen für die Zeichenfläche anpassen
        if (fLatMin > sAirspaces[dwA].fLatMin)
          fLatMin = sAirspaces[dwA].fLatMin;

        if (fLatMax < sAirspaces[dwA].fLatMax)
          fLatMax = sAirspaces[dwA].fLatMax;

        if (fLonMin > sAirspaces[dwA].fLonMin)
          fLonMin = sAirspaces[dwA].fLonMin;

        if (fLonMax < sAirspaces[dwA].fLonMax)
          fLonMax = sAirspaces[dwA].fLonMax;
      }
    }
  }
}

/****************************************************************************
 *                                                                          *
 * Programm: SelectDlgProc                                                  *
 *                                                                          *
 * Aufgabe : Auswahl-Dialog anzeigen                                        *
 *                                                                          *
 ****************************************************************************/
LRESULT CALLBACK SelectDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  char cValues[10]; // -180.0000 + '\0'

  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      return FALSE;
    }

    case WM_SHOWWINDOW:
    {
      sprintf(cValues, "%0.4f", fLatSelCenter);                               // Text aus Breitengrad erzeugen                            
      Edit_SetText(GetDlgItem(hDlg, IDC_SELECT_LAT), cValues);                // .. und im Editierfeld anzeigen
      sprintf(cValues, "%0.4f", fLonSelCenter);                               // Text aus Längengrad erzeugen
      Edit_SetText(GetDlgItem(hDlg, IDC_SELECT_LON), cValues);                // ... und im Editierfeld anzeigen
      sprintf(cValues, "%0.4f", fSelSize);                                    // Text aus Größe/Ausdehnung erzeugen
      Edit_SetText(GetDlgItem(hDlg, IDC_SELECT_SIZE), cValues);               // ... und im Editierfeld anzeigen

      if (SELECT_CIRCLE == bSelType)                                          // bei Auswahl = kreisförmig
      {
        CheckRadioButton(hDlg, IDC_SELECT_CIRCLE, IDC_SELECT_RECT, IDC_SELECT_CIRCLE);  //... Radio-Button "kreisförmig" selektieren
      }
      else if (SELECT_RECT == bSelType)                                       // bei Auswahl = rechteckig
      {
        CheckRadioButton(hDlg, IDC_SELECT_CIRCLE, IDC_SELECT_RECT, IDC_SELECT_RECT);  // ... Radio-Button "rechteckig" selektieren 
      }

      return TRUE;
    }

    case WM_COMMAND:

      switch (LOWORD(wParam))
      {
        case IDOK:

          Edit_GetText(GetDlgItem(hDlg, IDC_SELECT_LAT), cValues, sizeof(cValues) - 1); // Breitengrad aus Textfeld holen
          fLatSelCenter = strtof(cValues, NULL);                              // ... und in Gleitkommawert konvertieren
          Edit_GetText(GetDlgItem(hDlg, IDC_SELECT_LON), cValues, sizeof(cValues) - 1); // Längengrad aus Textfeld holen
          fLonSelCenter = strtof(cValues, NULL);                              // ... und in Gleitkommawert konvertieren
          Edit_GetText(GetDlgItem(hDlg, IDC_SELECT_SIZE), cValues, sizeof(cValues) - 1);  // Größe aus Textfeld holen
          fSelSize = strtof(cValues, NULL);                                   // ... und in Gleitkommawert konvertieren

          if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SELECT_CIRCLE))     // "kreisförmig" ausgewählt ?
          {
            bSelType = SELECT_CIRCLE;
          }
          else if (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_SELECT_RECT))  // "rechteckig" ausgewählt ?
          {
            bSelType = SELECT_RECT;
            SelectAirspaces(fLatSelCenter, fLonSelCenter, fSelSize, bSelType);
          }

          EndDialog(hDlg, TRUE);

          return TRUE;

        case IDCANCEL:

          EndDialog(hDlg, TRUE);

          return TRUE;

        case IDC_SELECT_CIRCLE:

          CheckRadioButton(hDlg, IDC_SELECT_CIRCLE, IDC_SELECT_RECT, IDC_SELECT_CIRCLE);// Radiobutton "kreisförmig" aktivieren

          return TRUE;

        case IDC_SELECT_RECT:

          CheckRadioButton(hDlg, IDC_SELECT_CIRCLE, IDC_SELECT_RECT, IDC_SELECT_RECT);  // Radiobutton "rechteckig" aktivieren

          return TRUE;

        default:

          return FALSE;
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
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  char szAppName[] = "OpenAir Viewer";
  char szAppClass[] = "OpenAirClass";

  WNDCLASS wc;
  MSG msg;

  ghInstance = hInstance;

  InitCommonControls();

  LoadLibrary("riched32.dll");

  wc.style = 0;
  wc.lpfnWndProc = (WNDPROC)WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN));
  wc.hCursor = 0;
  wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
  wc.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN);
  wc.lpszClassName = szAppClass;

  if (!RegisterClass(&wc))
    return 0;

  hApp = CreateWindow(szAppClass, szAppName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, ghInstance, NULL);

  if (!hApp)
    return 0;

  ShowWindow(hApp, nCmdShow);

  UpdateWindow(hApp);

  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}

/****************************************************************************
 *                                                                          *
 * Programm: ReadString                                                     *
 *                                                                          *
 * Aufgabe : Speichert einen String und gibt den Pointer darauf zurück      *
 *                                                                          *
 ****************************************************************************/
DWORD ReadString(char *szString)
{
  DWORD dwLen = strlen(szString) + 1;

  if (szString[dwLen - 2] == 10)
    szString[--dwLen - 1] = 0;

  pStrings = realloc(pStrings, dwStringsIdx + dwLen);

  if (pStrings == NULL)
    return (0);

  strncpy(pStrings + dwStringsIdx, szString, dwLen);

  DWORD dwPos = dwStringsIdx;

  dwStringsIdx += dwLen;

  return (dwPos);
}

/****************************************************************************
 *                                                                          *
 * Programm: LatLonStrToFloat                                               *
 *                                                                          *
 * Aufgabe : Eine Längen/Breitenangabe von string in float-Zahl umrechnen   *
 *                                                                          *
 *           pString: "GRAD:MINUTE:SEKUNDE (N/S/O/W)"                       *
 *                                                                          *
 *           fVal:    Koordinate im Dezimalformat als float                 *
 *                                                                          *
 *           Südliche Breite/Westliche Länge sind negativ!!!                *
 *                                                                          *
 ****************************************************************************/
float LatLonStrToFloat(char *pString)
{
  char *pPtr = pString;
  float fVal = 0.0, fDiv = 1.0;
  BYTE bI;

  for (bI = 0; bI < 4; bI++)
  {
    char *pRet = strtok(pPtr, " :,\n");

    if (pRet == NULL)
      return (0.0);

    if (strlen(pRet) == 1 && strchr("NnSsWwEeOo", *pRet) != NULL)
    {
      if (strchr("SsWw", *pRet) != NULL)
        fVal = -fVal;

      return (fVal);
    }

    fVal += atof(pRet) / fDiv;

    fDiv *= 60.0;
    pPtr = NULL;
  }
  return (0.0);
}

/****************************************************************************
 *                                                                          *
 * Programm: LatLonFloatToStr                                               *
 *                                                                          *
 * Aufgabe : Eine Längen/Breitenangabe von float in einen string umrechnen  *
 *                                                                          *
 *           fKoor: Koordinate im Dezimalformat als float                   *
 *           bLon: TRUE - Längenangabe / FALSE - Breitenangabe              *
 *                                                                          *
 *           pString: "GRAD:MINUTE:SEKUNDE (N/S/O/W)"                       *
 *           iLen: Länge des erzeugten Texts                                *
 *                                                                          *
 *           Südliche Breite/Westliche Länge sind negativ!!!                *
 *                                                                          *
 ****************************************************************************/
int LatLonFloatToStr(float fKoor, BOOL bLon, char *pString)
{
  float fGrd, fMin, fSek;
  char cLon;
  char cLat;
  int iLen = 0;
  BOOL bNeg = FALSE;

  bNeg = FALSE;
  if (fKoor < 0)
  {
    bNeg = TRUE;
    fKoor *= -1.0f;
  }

  fGrd = truncf(fKoor);
  fKoor = (fKoor - fGrd) * 60.0f;
  fMin = truncf(fKoor);
  fKoor = (fKoor - fMin) * 60.0f;
  fSek = round(fKoor);
  if (fSek > 59.5f)
  {
    fSek = 0.0f;
    fMin = fMin + 1.0f;
  }

  if (TRUE == bLon)
  {
    if (TRUE == bNeg)
    {
      cLon = 'W';
    }
    else
    {
      cLon = 'E';
    }
    iLen = sprintf(pString, "%03.0f:%02.0f:%02.0f %c", fGrd, fMin, fSek, cLon); // geographische Länge z.B. 013:00:00 E
  }
  else
  {
    if (TRUE == bNeg)
    {
      cLat = 'S';
    }
    else
    {
      cLat = 'N';
    }
    iLen = sprintf(pString, "%02.0f:%02.0f:%02.0f %c", fGrd, fMin, fSek, cLat); // geographische Breite z.B. 52:00:00 N
  }
  return iLen;
}

/****************************************************************************
 *                                                                          *
 * Programm: SetMax                                                         *
 *                                                                          *
 * Aufgabe : Setze die maximale Ausdehnung (oder Initialwerte) für          *
 *           einen Luftraum                                                 *
 *                                                                          *
 ****************************************************************************/
void SetMax(float fLat, float fLon, float fRad)
{
  if (0 == sAirspaces[dwAirspaceZ - 1].dwElemAnz)                             // Noch keine Unterelemente zum Luftraum vorhanden ?
  {                                                                           // Dann Übergabe-Parameter als Startwerte übernehmen. 
    sAirspaces[dwAirspaceZ - 1].dwElemIdx = dwContZ;
    sAirspaces[dwAirspaceZ - 1].fLatMin = fLat - fRad;
    sAirspaces[dwAirspaceZ - 1].fLatMax = fLat + fRad;
    sAirspaces[dwAirspaceZ - 1].fLonMin = fLon - fRad;
    sAirspaces[dwAirspaceZ - 1].fLonMax = fLon + fRad;
  }
  else
  {
    if (sAirspaces[dwAirspaceZ - 1].fLatMin > (fLat - fRad))
      sAirspaces[dwAirspaceZ - 1].fLatMin = fLat - fRad;

    if (sAirspaces[dwAirspaceZ - 1].fLatMax < (fLat + fRad))
      sAirspaces[dwAirspaceZ - 1].fLatMax = fLat + fRad;

    if (sAirspaces[dwAirspaceZ - 1].fLonMin > (fLon - fRad))
      sAirspaces[dwAirspaceZ - 1].fLonMin = fLon - fRad;

    if (sAirspaces[dwAirspaceZ - 1].fLonMax < (fLon + fRad))
      sAirspaces[dwAirspaceZ - 1].fLonMax = fLon + fRad;
  }
}

/****************************************************************************
 *                                                                          *
 * Programm: Dist                                                           *
 *                                                                          *
 * Aufgabe : Berechnet eine Distanz                                         *
 *                                                                          *
 ****************************************************************************/
float Dist(float fDeltaX, float fDeltaY)
{
  return (sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY));
}

/****************************************************************************
 *                                                                          *
 * Programm: BuildTreeView                                                  *
 *                                                                          *
 * Aufgabe : Erzeugt die Baumstruktur mit den Lufträumen. Es werden nur     *
 *           Lufträume hinzugefügt, die gültig sind (bValid).               *
 *                                                                          *
 ****************************************************************************/
void BuildTreeView(void)
{
  TV_INSERTSTRUCT sInsert;  // TreeView-Eintrag
  char szBuffer[18];        // Text zum Eintrag
  DWORD dwAirIdx;           // ein Index für Eintrag in sCont vom Typ Luftraum
  DWORD dwIdx, dwElemIdx;
  BOOL bNewPoint = FALSE;

  TreeView_SelectItem(hWndTV, 0);

  TreeView_DeleteAllItems(hWndTV);

  memset(&sInsert, 0, sizeof(sInsert));

  // OA_TYP_FILE
  sInsert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
  sInsert.item.cchTextMax = MAX_PATH;
  sInsert.item.pszText = strrchr(szFile, '\\') + 1;
  sInsert.item.iImage = 1;
  sCont[0].hParent = TreeView_InsertItem(hWndTV, &sInsert);

  for (dwAirIdx = 1; dwAirIdx < dwContZ; dwAirIdx++)
  {
    if ((OA_TYP_AIRSPACE == sCont[dwAirIdx].bTyp) &&
        (TRUE == sAirspaces[sCont[dwAirIdx].dwIndex].bValid))
    {
      // Luftraum
      sInsert.hParent = sCont[0].hParent;
      sInsert.item.lParam = dwAirIdx;
      sInsert.item.pszText = pStrings + sAirspaces[sCont[dwAirIdx].dwIndex].dwName;
      sInsert.item.iSelectedImage = TV_AIRSPACE_ICON;
      sInsert.item.iImage = TV_AIRSPACE_ICON + 1;
      sCont[dwAirIdx].hParent = TreeView_InsertItem(hWndTV, &sInsert);
      bNewPoint = TRUE;

      for (dwIdx = 0; dwIdx < sAirspaces[sCont[dwAirIdx].dwIndex].dwElemAnz; dwIdx++)
      {
        dwElemIdx = sAirspaces[sCont[dwAirIdx].dwIndex].dwElemIdx + dwIdx;    // der Index für den Zugriff auf den Container
        switch (sCont[dwElemIdx].bTyp)
        {
          // Punkt
          case OA_TYP_POINT:
            sprintf(szBuffer, "%03lu Punkt", dwIdx + 1);
            sInsert.hParent = sCont[dwAirIdx].hParent;
            sInsert.item.lParam = dwElemIdx;
            sInsert.item.pszText = szBuffer;
            if (TRUE == bNewPoint)
            {
              sInsert.item.iSelectedImage = TV_NEW_POINT_ICON;
              sInsert.item.iImage = TV_NEW_POINT_ICON + 1;
            }
            else
            {
              sInsert.item.iSelectedImage = TV_POINT_ICON;
              sInsert.item.iImage = TV_POINT_ICON + 1;
            }
            bNewPoint = FALSE;
            sCont[dwElemIdx].hParent = TreeView_InsertItem(hWndTV, &sInsert);
            break;

          // Kreis
          case OA_TYP_CIRCLE:
            sprintf(szBuffer, "%03lu Kreis", dwIdx + 1); 
            sInsert.hParent = sCont[dwAirIdx].hParent;
            sInsert.item.lParam = dwElemIdx;
            sInsert.item.pszText = szBuffer;
            sInsert.item.iSelectedImage = TV_CIRCLE_ICON;
            sInsert.item.iImage = TV_CIRCLE_ICON + 1;
            sCont[dwElemIdx].hParent = TreeView_InsertItem(hWndTV, &sInsert);
            break;

          // Kreisbogen mit Koordinaten
          case OA_TYP_ARC:
            sprintf(szBuffer, "%03lu Kreisbogen", dwIdx + 1);
            sInsert.hParent = sCont[dwAirIdx].hParent;
            sInsert.item.lParam = dwElemIdx;
            sInsert.item.pszText = szBuffer;
            sInsert.item.iSelectedImage = TV_ARC_ICON;
            sInsert.item.iImage = TV_ARC_ICON + 1;
            sCont[dwElemIdx].hParent = TreeView_InsertItem(hWndTV, &sInsert);
            break;

          default:
            break;
        }
      }
    }
  }

  TreeView_SelectItem(hWndTV, sCont[0].hParent);
  SetFocus(hWndTV);
}

/****************************************************************************
 *                                                                          *
 * Programm: ReadOAFile                                                     *
 *                                                                          *
 * Aufgabe : Liest die OpenAirDatei und erzeugt den TreeView                *
 *                                                                          *
 ****************************************************************************/
void ReadOAFile(char *szFile)
{
  char szBuffer[256];
  FILE *pFile;
  DWORD dwAktAirspace = 0;
  BOOL bNewAirspace = TRUE, bNewPoint = TRUE;
  BYTE bRotDir = OA_ROT_DIR_LEFT;
  float fCircleLon = 52.0, fCircleLat = 13.0;

  TreeView_SelectItem(hWndTV, 0);

  TreeView_DeleteAllItems(hWndTV);

  if ((pFile = fopen(szFile, "rt")) == NULL)
  {
    MessageBox(hApp, "OpenAir Datei konnte nicht geöffnet werden", "Datei öffnen", MB_ICONEXCLAMATION);
    return;
  }

  dwContZ = dwAirspaceZ = dwPointZ = dwCircleZ = dwStringsIdx = 0;

  if (sCont != NULL)
    free(sCont);
  if (sAirspaces != NULL)
    free(sAirspaces);
  if (sPoints != NULL)
    free(sPoints);
  if (sCircles != NULL)
    free(sCircles);
  if (pStrings != NULL)
    free(pStrings);

  sAirspaces = NULL;
  sPoints = NULL;
  sCircles = NULL;
  pStrings = NULL;

  ReadString("<NULL>");

  dwContZ = dwAirspaceZ = dwPointZ = dwCircleZ = 0;

  sCont = malloc(sizeof(struct strContainer));

  sCont[0].bTyp = OA_TYP_FILE;

  dwContZ = 1;

  while (!feof(pFile))
  {
    if (!fgets(szBuffer, 255, pFile))
      continue;

    switch (szBuffer[0])
    {
      case 'A':  // Airspace ...
      {
        if (bNewAirspace)
        {
          sAirspaces = realloc(sAirspaces, (dwAirspaceZ + 1) * sizeof(struct strAirspace));

          memset(&sAirspaces[dwAirspaceZ], 0, sizeof(struct strAirspace));

          sAirspaces[dwAirspaceZ].bValid = TRUE;                              // neu geladene Lufträume sind immer gültig

          bNewAirspace = FALSE;
          bNewPoint = TRUE;

          bRotDir = OA_ROT_DIR_DEFAULT;
        }

        switch (szBuffer[1])
        {
          case 'C':  // AC Class
          {
            sAirspaces[dwAirspaceZ].dwClass = ReadString(szBuffer + 3);
            break;
          }

          case 'N':  // AN Name
          {
            sAirspaces[dwAirspaceZ].dwName = ReadString(szBuffer + 3);
            break;
          }

          case 'H':  // AH High
          {
            sAirspaces[dwAirspaceZ].dwHigh = ReadString(szBuffer + 3);
            break;
          }

          case 'L':  // AL Low
          {
            sAirspaces[dwAirspaceZ].dwLow = ReadString(szBuffer + 3);
            break;
          }
        }
        break;

      }

      case 'D': // Angaben zu Airspace, Point, Circle, Arc starten mit D
      {         // Ausnahme V -> Angabe des Mittelpunkts bei Circle und Arc
        if (!bNewAirspace)
        {
          dwAktAirspace = dwContZ;

          dwContZ++;

          sCont = realloc(sCont, (dwContZ) * sizeof(struct strContainer));

          sCont[dwAktAirspace].bTyp = OA_TYP_AIRSPACE;
          sCont[dwAktAirspace].dwIndex = dwAirspaceZ;

          dwAirspaceZ++;

          bNewAirspace = TRUE;
        }

        switch (szBuffer[1])
        {
          case 'A': // DA Kreisbogen mit Winkeln
          {
            float fWinkel;

            sCircles = realloc(sCircles, (dwCircleZ + 1) * sizeof(struct strCircle));

            sCircles[dwCircleZ].bRotDir = bRotDir;
            sCircles[dwCircleZ].dwAirspace = dwAktAirspace;
            sCircles[dwCircleZ].fRad = atof(strtok(szBuffer + 3, " :,\n")) / 60.0;

            fWinkel = atof(strtok(szBuffer + 3, " :,\n")) / RHO;

            sCircles[dwCircleZ].fLatB = sin(fWinkel) * sCircles[dwCircleZ].fRad + fCircleLat;
            sCircles[dwCircleZ].fLonB = cos(fWinkel) * sCircles[dwCircleZ].fRad + fCircleLon;

            fWinkel = atof(strtok(szBuffer + 3, " :,\n")) / RHO;

            sCircles[dwCircleZ].fLatE = sin(fWinkel) * sCircles[dwCircleZ].fRad + fCircleLat;
            sCircles[dwCircleZ].fLonE = cos(fWinkel) * sCircles[dwCircleZ].fRad + fCircleLon;
            sCircles[dwCircleZ].fLatM = fCircleLat;
            sCircles[dwCircleZ].fLonM = fCircleLon;

            sCont = realloc(sCont, (dwContZ + 1) * sizeof(struct strContainer));

            sCont[dwContZ].bTyp = OA_TYP_ARC;
            sCont[dwContZ].dwIndex = dwCircleZ;

            SetMax(sCircles[dwCircleZ].fLatM, sCircles[dwCircleZ].fLonM, sCircles[dwCircleZ].fRad);

            sAirspaces[dwAirspaceZ - 1].dwElemAnz++;

            dwContZ++;
            dwCircleZ++;

            bNewPoint = FALSE;

            break;
          }

          case 'B': // DB Kreisbogen mit Koordinaten
          {
            sCircles = realloc(sCircles, (dwCircleZ + 1) * sizeof(struct strCircle));

            sCircles[dwCircleZ].bRotDir = bRotDir;
            sCircles[dwCircleZ].dwAirspace = dwAktAirspace;

            float fCos = cos(fCircleLat / RHO);

            sCircles[dwCircleZ].fLatB = LatLonStrToFloat(szBuffer + 3);
            sCircles[dwCircleZ].fLonB = LatLonStrToFloat(NULL);
            sCircles[dwCircleZ].fLatE = LatLonStrToFloat(NULL);
            sCircles[dwCircleZ].fLonE = LatLonStrToFloat(NULL);
            sCircles[dwCircleZ].fRad = (Dist(sCircles[dwCircleZ].fLatB - fCircleLat, (sCircles[dwCircleZ].fLonB - fCircleLon) * fCos) + Dist(sCircles[dwCircleZ].fLatE - fCircleLat, (sCircles[dwCircleZ].fLonE - fCircleLon) * fCos)) / 2.0;
            sCircles[dwCircleZ].fLatM = fCircleLat;
            sCircles[dwCircleZ].fLonM = fCircleLon;

            sCont = realloc(sCont, (dwContZ + 1) * sizeof(struct strContainer));

            sCont[dwContZ].bTyp = OA_TYP_ARC;
            sCont[dwContZ].dwIndex = dwCircleZ;

            SetMax(sCircles[dwCircleZ].fLatM, sCircles[dwCircleZ].fLonM, sCircles[dwCircleZ].fRad);

            sAirspaces[dwAirspaceZ - 1].dwElemAnz++;

            dwContZ++;
            dwCircleZ++;

            bNewPoint = FALSE;

            break;
          }

          case 'C': // DC Vollkreis
          {
            sCircles = realloc(sCircles, (dwCircleZ + 1) * sizeof(struct strCircle));

            sCircles[dwCircleZ].dwAirspace = dwAktAirspace;
            sCircles[dwCircleZ].fRad = atof(szBuffer + 3) / 60.0;
            sCircles[dwCircleZ].fLatM = fCircleLat;
            sCircles[dwCircleZ].fLonM = fCircleLon;

            sCont = realloc(sCont, (dwContZ + 1) * sizeof(struct strContainer));

            sCont[dwContZ].bTyp = OA_TYP_CIRCLE;
            sCont[dwContZ].dwIndex = dwCircleZ;

            SetMax(sCircles[dwCircleZ].fLatM, sCircles[dwCircleZ].fLonM, sCircles[dwCircleZ].fRad);

            sAirspaces[dwAirspaceZ - 1].dwElemAnz++;

            dwContZ++;
            dwCircleZ++;

            bNewPoint = TRUE;

            break;
          }

          case 'P': // DP Point
          {
            sPoints = realloc(sPoints, (dwPointZ + 1) * sizeof(struct strPoint));

            sPoints[dwPointZ].dwAirspace = dwAktAirspace;
            sPoints[dwPointZ].fLat = LatLonStrToFloat(szBuffer + 3);
            sPoints[dwPointZ].fLon = LatLonStrToFloat(NULL);

            sCont = realloc(sCont, (dwContZ + 1) * sizeof(struct strContainer));

            sCont[dwContZ].bTyp = OA_TYP_POINT;
            sCont[dwContZ].dwIndex = dwPointZ;

            SetMax(sPoints[dwPointZ].fLat, sPoints[dwPointZ].fLon, 0);

            sAirspaces[dwAirspaceZ - 1].dwElemAnz++;

            dwContZ++;
            dwPointZ++;

            bNewPoint = FALSE;

            break;
          }
        }
      }

      case 'V': // Kreis/Kreisbogen Parameter
      {
        char *pPtr = szBuffer + 1;

        while (*pPtr == ' ')
          pPtr++;

        if (pPtr[1] == '=')
        {
          if (*pPtr == 'D' || *pPtr == 'd')
          {
            if (pPtr[2] == '-')
            {
              bRotDir = OA_ROT_DIR_LEFT;
            }
            else if (pPtr[2] == '+')
            {
              bRotDir = OA_ROT_DIR_RIGHT;
            }
          }
          else if (*pPtr == 'X' || *pPtr == 'x')
          {
            fCircleLat = LatLonStrToFloat(pPtr + 2);
            fCircleLon = LatLonStrToFloat(NULL);
          }
        }
      }
    }
  }
  fclose(pFile);

  if (dwAirspaceZ)
  {
    fLatMin = sAirspaces[0].fLatMin;
    fLatMax = sAirspaces[0].fLatMax;
    fLonMin = sAirspaces[0].fLonMin;
    fLonMax = sAirspaces[0].fLonMax;

    DWORD dwI;

    for (dwI = 1; dwI < dwAirspaceZ; dwI++)
    {
      if (fLatMin > sAirspaces[dwI].fLatMin)
        fLatMin = sAirspaces[dwI].fLatMin;

      if (fLatMax < sAirspaces[dwI].fLatMax)
        fLatMax = sAirspaces[dwI].fLatMax;

      if (fLonMin > sAirspaces[dwI].fLonMin)
        fLonMin = sAirspaces[dwI].fLonMin;

      if (fLonMax < sAirspaces[dwI].fLonMax)
        fLonMax = sAirspaces[dwI].fLonMax;
    }
  }

  sprintf(szBuffer, "%lu Lufräume mit %lu Punkten und %lu Kreisen bzw. Kreisbögen", dwAirspaceZ, dwPointZ, dwCircleZ);

  SendDlgItemMessage(hApp, IDC_STBA, SB_SETTEXT, 0, (LPARAM)szBuffer); 

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: SaveOAFile                                                     *
 *                                                                          *
 * Aufgabe : Speichert die OpenAirDatei                                     *
 *                                                                          *
 ****************************************************************************/
void SaveOAFile(char *szFile)
{
  char szBuffer[256];
  int iOffset;
  FILE *pFile;
  DWORD dwA = 0;
  DWORD dwC = 0;
  BYTE bRotDir;

  TreeView_SelectItem(hWndTV, 0);

  if ((pFile = fopen(szFile, "wt")) == NULL)                                  // als Textdatei speichern
  {
    MessageBox(hApp, "OpenAir Datei konnte nicht gespeichert werden", "Datei speichern", MB_ICONEXCLAMATION);

    return;
  }

  // Reihenfolge der Lufträume
  // *####### BEGIN AIRSPACE -RESTRICTED- ########     ED-R
  // *####### BEGIN AIRSPACE -DANGER AREAS- ########   ED-D
  // *####### BEGIN AIRSPACE -CTR- ########            CTR
  // *####### BEGIN AIRSPACE D -NOT CTR- ########      D
  // *####### BEGIN AIRSPACE -RMZ- ########            RMZ
  // *####### BEGIN AIRSPACE -TMZ- ########            TMZ
  // *####### BEGIN AIRSPACE -GLIDER SECTORS- ######## W 

  for (dwA = 0; dwA < dwAirspaceZ; dwA++)
  {
    if (TRUE == sAirspaces[dwA].bValid)
    {
      // Luftrauminfos
      // AC R
      // AN ED-R116 Baumholder
      // AH FL95
      // AL 1500ft AGL
      //
      sprintf(szBuffer, "AC %s\n", pStrings + sAirspaces[dwA].dwClass);
      fputs(szBuffer, pFile);
      sprintf(szBuffer, "AN %s\n", pStrings + sAirspaces[dwA].dwName);
      fputs(szBuffer, pFile);
      sprintf(szBuffer, "AH %s\n", pStrings + sAirspaces[dwA].dwHigh);
      fputs(szBuffer, pFile);
      sprintf(szBuffer, "AL %s\n", pStrings + sAirspaces[dwA].dwLow);
      fputs(szBuffer, pFile);

      bRotDir = OA_ROT_DIR_DEFAULT;

      // Koordinaten in GRAD:MINUTE:SEKUNDE N/S GRAD:MINUTE:SEKUNDE W/E
      // Geographische Breite + = Nord, - = Süd
      // Geographische Länge + = Ost, - = West     
      for (dwC = sAirspaces[dwA].dwElemIdx; dwC < (sAirspaces[dwA].dwElemIdx + sAirspaces[dwA].dwElemAnz); dwC++)
      {
        switch (sCont[dwC].bTyp)
        {
          // Punkt
          // DP 49:39:42 N 007:00:21 E                                     
          case OA_TYP_POINT:
            iOffset = sprintf(szBuffer, "DP ");

            iOffset += LatLonFloatToStr(sPoints[sCont[dwC].dwIndex].fLat, FALSE, szBuffer + iOffset);
            iOffset += sprintf(szBuffer + iOffset, " ");
            iOffset += LatLonFloatToStr(sPoints[sCont[dwC].dwIndex].fLon, TRUE, szBuffer + iOffset);

            sprintf(szBuffer + iOffset, "\n");

            fputs(szBuffer, pFile);
            break;

          // Kreis
          // V X=53:24:25 N 010:25:10 E                         Mittelpunkt
          // DC 1.10                                            Radius (Nautische Meilen)
          case OA_TYP_CIRCLE:
            iOffset = sprintf(szBuffer, "V X=");

            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLatM, FALSE, szBuffer + iOffset);
            iOffset += sprintf(szBuffer + iOffset, " ");
            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLonM, TRUE, szBuffer + iOffset);

            sprintf(szBuffer + iOffset, "\n");

            fputs(szBuffer, pFile);

            sprintf(szBuffer, "DC %0.2f\n", (sCircles[sCont[dwC].dwIndex].fRad * 60.0f));

            fputs(szBuffer, pFile);
            break;

          // Kreisbogen mit Koordinaten
          // V D=-                                              Richtung (linksdrehend)
          // V X=52:24:38 N 013:07:46 E                         Mittelpunkt
          // DB 52:22:39 N 013:08:15 E , 52:24:33 N 013:11:02 E Koordinaten von-bis
          case OA_TYP_ARC:
            // es gibt 2 Drehrichtungen, linksrum und rechtsrum
            // wenn die Drehrichtung nicht gesetzt ist (OA_ROT_DIR_DEFAULT), 
            // wird keine Drehrichtung in die Datei geschrieben
            if (bRotDir != sCircles[sCont[dwC].dwIndex].bRotDir)
            {
              if (OA_ROT_DIR_LEFT == sCircles[sCont[dwC].dwIndex].bRotDir)
              {
                sprintf(szBuffer, "V D=-\n");
                fputs(szBuffer, pFile);
                bRotDir = OA_ROT_DIR_LEFT;
              }
              else if (OA_ROT_DIR_RIGHT == sCircles[sCont[dwC].dwIndex].bRotDir)
              {
                sprintf(szBuffer, "V D=+\n");
                fputs(szBuffer, pFile);
                bRotDir = OA_ROT_DIR_RIGHT;
              }
            }

            iOffset = sprintf(szBuffer, "V X=");

            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLatM, FALSE, szBuffer + iOffset);
            iOffset += sprintf(szBuffer + iOffset, " ");
            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLonM, TRUE, szBuffer + iOffset);

            sprintf(szBuffer + iOffset, "\n");

            fputs(szBuffer, pFile);

            iOffset = sprintf(szBuffer, "DB ");

            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLatB, FALSE, szBuffer + iOffset);
            iOffset += sprintf(szBuffer + iOffset, " ");
            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLonB, TRUE, szBuffer + iOffset);

            iOffset += sprintf(szBuffer + iOffset, " , ");

            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLatE, FALSE, szBuffer + iOffset);
            iOffset += sprintf(szBuffer + iOffset, " ");
            iOffset += LatLonFloatToStr(sCircles[sCont[dwC].dwIndex].fLonE, TRUE, szBuffer + iOffset);

            sprintf(szBuffer + iOffset, "\n");

            fputs(szBuffer, pFile);
            break;

          default:
            break;
        }
      }

      sprintf(szBuffer, "\n");                                                // nächste Zeile
      fputs(szBuffer, pFile);
    }
  }

  fclose(pFile);

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: TextPosOut                                                     *
 *                                                                          *
 * Aufgabe : Einen Text positioniert auf der Zeichenfläche ausgeben         *
 *                                                                          *
 ****************************************************************************/
void TextPosOut(HDC hDC, int nX, int nY, int nMidX, int nMidY, DWORD dwE)
{
#define OFFSET 2

  WORD wAlign;

  if (nX > nMidX)
  {
    wAlign = TA_LEFT;

    nX += OFFSET;
  }
  else
  {
    wAlign = TA_RIGHT;

    nX -= OFFSET;
  }

  if (nY < nMidY)
  {
    wAlign += TA_BOTTOM;

    nY -= OFFSET;
  }
  else
  {
    wAlign += TA_TOP;

    nY += OFFSET;
  }

  SetTextAlign(hDC, wAlign);

  char szBuffer[5];

  sprintf(szBuffer, " %lu", dwE + 1);

  TextOut(hDC, nX, nY, szBuffer, strlen(szBuffer));

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: ViewAirspace                                                   *
 *                                                                          *
 * Aufgabe : Darstellungsroutine eines Luftraums                            *
 *                                                                          *
 *           Die Farbe der Luftraums ist abhängig vom Typ (dwClass)         *
 *                                                                          *
 ****************************************************************************/
void ViewAirspace(HDC hDC, DWORD dwIndex, DWORD dwElem, BOOL bMark, int iPenSize)
{
  HPEN hPen;
  DWORD dwE;
  BOOL bStart = FALSE;
  int nXs = 0, nYs = 0;
  char *sClass;

  sClass = pStrings + sAirspaces[dwIndex].dwClass;
  if ((strncmp(sClass, "Q", 1) == 0) ||                                       // ED-D
      (strncmp(sClass, "R", 1) == 0) ||                                       // ED-R
      (strncmp(sClass, "TMZ", 3) == 0))                                       // TMZ
  {
    hPen = CreatePen(PS_SOLID, iPenSize, RGB(0xFF, 0x00, 0x00));              // -> rot
  }
  else if ((strncmp(sClass, "CTR", 3) == 0) ||                                // CTR
           (strncmp(sClass, "RMZ", 3) == 0) ||                                // RMZ
           (strncmp(sClass, "C", 1) == 0))                                    // C
  {
    hPen = CreatePen(PS_SOLID, iPenSize, RGB(0xAA, 0x00, 0xFF));              // -> lila
  }
  else if (strncmp(sClass, "D", 1) == 0)                                      // D
  {
    hPen = CreatePen(PS_SOLID, iPenSize, RGB(0x00, 0x00, 0xFF));              // -> blau
  }
  else
  {                                                                           // alle anderen
    hPen = CreatePen(PS_SOLID, iPenSize, RGB(0xAA, 0xAA, 0xAA));              // -> grau
  }
  SelectObject(hDC, hPen);

  for (dwE = 0; dwE < sAirspaces[dwIndex].dwElemAnz; dwE++)
  {
    DWORD dwIdx = sCont[sAirspaces[dwIndex].dwElemIdx + dwE].dwIndex;

    switch (sCont[sAirspaces[dwIndex].dwElemIdx + dwE].bTyp)
    {
      case OA_TYP_POINT:
      {
        int nX = nMidX + (sPoints[dwIdx].fLon - fMidLon) / fDivLon;
        int nY = nMidY - (sPoints[dwIdx].fLat - fMidLat) / fDivLat;

        if (!bStart)
        {
          nXs = nX;
          nYs = nY;
          bStart = TRUE;

          MoveToEx(hDC, nX, nY, NULL);
        }
        else
        {
          LineTo(hDC, nX, nY);
        }

        if (bMark)
        {
          if ((dwE + 1) == dwElem)
          {
            SetTextColor(hDC, RGB(200, 0, 0));
            TextPosOut(hDC, nX, nY, nMidX, nMidY, dwE);
            SetTextAlign(hDC, TA_CENTER);
            TextOut(hDC, nX, nY - 9, "o", 1);
            SetTextColor(hDC, RGB(0, 0, 200));
          }
          else
          {
            TextPosOut(hDC, nX, nY, nMidX, nMidY, dwE);
          }
        }
        break;
      }

      case OA_TYP_ARC:
      {
        int nXm = nMidX + (sCircles[dwIdx].fLonM - fMidLon) / fDivLon;
        int nYm = nMidY - (sCircles[dwIdx].fLatM - fMidLat) / fDivLat;
        int nR = sCircles[dwIdx].fRad / fDivLat;
        int nXb = nMidX + (sCircles[dwIdx].fLonB - fMidLon) / fDivLon;
        int nYb = nMidY - (sCircles[dwIdx].fLatB - fMidLat) / fDivLat;
        int nXe = nMidX + (sCircles[dwIdx].fLonE - fMidLon) / fDivLon;
        int nYe = nMidY - (sCircles[dwIdx].fLatE - fMidLat) / fDivLat;

        if (OA_ROT_DIR_LEFT == sCircles[dwIdx].bRotDir)
        {
          SetArcDirection(hDC, AD_COUNTERCLOCKWISE);
        }
        else
        {
          SetArcDirection(hDC, AD_CLOCKWISE);
        }

        if (!bStart)
        {
          nXs = nXb;
          nYs = nYb;
          bStart = TRUE;

          MoveToEx(hDC, nXb, nYb, NULL);
        }

        ArcTo(hDC, nXm - nR, nYm - nR, nXm + nR, nYm + nR, nXb, nYb, nXe, nYe);

        if (bMark)
        {
          if ((dwE + 1) == dwElem)
          {
            SetTextColor(hDC, RGB(200, 0, 0));
            TextPosOut(hDC, nXe, nYe, nMidX, nMidY, dwE);
            SetTextAlign(hDC, TA_CENTER);
            TextOut(hDC, nXe, nYe - 9, "o", 1);
            SetTextColor(hDC, RGB(0, 0, 200));
          }
          else
          {
            TextPosOut(hDC, nXe, nYe, nMidX, nMidY, dwE);
          }
        }
        break;
      }

      case OA_TYP_CIRCLE:
      {
        int nX = nMidX + (sCircles[dwIdx].fLonM - fMidLon) / fDivLon;
        int nY = nMidY - (sCircles[dwIdx].fLatM - fMidLat) / fDivLat;
        int nR = sCircles[dwIdx].fRad / fDivLat;

        Ellipse(hDC, nX - nR, nY - nR, nX + nR, nY + nR);

        if (bMark)
        {
          if ((dwE + 1) == dwElem)
          {
            SetTextColor(hDC, RGB(250, 0, 0));
            TextPosOut(hDC, nX, nY, nMidX, nMidY, dwE);
            SetTextAlign(hDC, TA_CENTER);
            TextOut(hDC, nX, nY - 9, "o", 1);
            SetTextColor(hDC, RGB(0, 0, 200));
          }
          else
          {
            TextPosOut(hDC, nX, nY, nMidX, nMidY, dwE);
          }
        }
        break;
      }
    }
  }
  
  if (bStart)
    LineTo(hDC, nXs, nYs);

  DeleteObject(hPen);

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: View                                                           *
 *                                                                          *
 * Aufgabe : Die Darstellungsroutine                                        *
 *                                                                          *
 *           Die Darstellung ist abhängig von dem, was im TreeView          *
 *           ausgewählt ist.                                                *
 *                                                                          *
 *           1. OA_TYP_FILE -> alle Lufträume, ohne graphische Selektion    *
 *           2. OA_TYP_AIRSPACE -> alle Lufträume, Selektion: breite Linie  *
 *           3. OA_TYP_POINT -> ein Lufraum, Selektion des Punktes          *
 *           4. OA_TYP_ARC -> ein Luftraum, Selektion des Startpunkts       *
 *           5. OA_TYP_CIRCLE -> ein Luftraum, Selektion des Mittelpunkts   *
 *                                                                          *
 ****************************************************************************/
void View(HWND hWnd)
{
  PAINTSTRUCT pS;
  HDC hDC;

  hDC = BeginPaint(hWnd, (LPPAINTSTRUCT) & pS);

  HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0x96, 0x96, 0x96));                  // hellgrau

  SelectObject(hDC, hPen);
  SelectObject(hDC, GetStockObject(WHITE_BRUSH));

  Rectangle(hDC, sRectView.left + 1, sRectView.top + 1, sRectView.right - 2, sRectView.bottom - 2);

  DeleteObject(hPen);

  if (!dwContZ)
  {
    EndPaint(hWnd, (LPPAINTSTRUCT) & pS);
    return;
  }

  hPen = CreatePen(PS_SOLID, 2, RGB(0x64, 0x64, 0x64));                       // dunkelgrau

  SelectObject(hDC, hPen);

  SetROP2(hDC, R2_MASKPEN);

  int nDelX = (sRectView.right - sRectView.left);
  int nDelY = (sRectView.bottom - sRectView.top);

  nMidX = sRectView.left + nDelX / 2;
  nMidY = sRectView.top + nDelY / 2;

  SetTextColor(hDC, RGB(0x00, 0x00, 0x00));                                   // schwarz

  SetBkMode(hDC, TRANSPARENT);

  switch (sCont[dwTreeViewSel].bTyp)
  {
    case OA_TYP_FILE:
    {
      float fDelLat = fLatMax - fLatMin;
      float fDelLon = fLonMax - fLonMin;

      fMidLat = fLatMin + fDelLat / 2.0;
      fMidLon = fLonMin + fDelLon / 2.0;

      float fKorr = cos(fMidLat / RHO);

      fDivLat = max(fDelLon * fKorr / nDelX, fDelLat / nDelY) * 1.5;
      fDivLon = fDivLat / fKorr;

      DWORD dwI;

      for (dwI = 0; dwI < dwAirspaceZ; dwI++)
      {
        if (TRUE == sAirspaces[dwI].bValid)
        {
          ViewAirspace(hDC, dwI, 0, FALSE, PEN_SIZE_NORMAL);
        }
      }
      break;
    }

    case OA_TYP_AIRSPACE:
    {
      float fDelLat = fLatMax - fLatMin;
      float fDelLon = fLonMax - fLonMin;

      fMidLat = fLatMin + fDelLat / 2.0;
      fMidLon = fLonMin + fDelLon / 2.0;

      float fKorr = cos(fMidLat / RHO);

      fDivLat = max(fDelLon * fKorr / nDelX, fDelLat / nDelY) * 1.5;
      fDivLon = fDivLat / fKorr;

      DWORD dwI;

      for (dwI = 0; dwI < dwAirspaceZ; dwI++)
      {
        if (dwI == sCont[dwTreeViewSel].dwIndex)
        {
          ViewAirspace(hDC, dwI, 0, FALSE, PEN_SIZE_SELECT);
        }
        else
        {
          if (TRUE == sAirspaces[dwI].bValid)
          {
            ViewAirspace(hDC, dwI, 0, FALSE, PEN_SIZE_NORMAL);
          }
        }
      }
      break;
    }

    case OA_TYP_POINT:
    {
      DWORD dwElem = sCont[dwTreeViewSel].dwIndex;
      DWORD dwCont = sPoints[dwElem].dwAirspace;
      DWORD dwIndex = sCont[dwCont].dwIndex;

      float fDelLat = sAirspaces[dwIndex].fLatMax - sAirspaces[dwIndex].fLatMin;
      float fDelLon = sAirspaces[dwIndex].fLonMax - sAirspaces[dwIndex].fLonMin;

      fMidLat = sAirspaces[dwIndex].fLatMin + fDelLat / 2.0;
      fMidLon = sAirspaces[dwIndex].fLonMin + fDelLon / 2.0;

      float fKorr = cos(fMidLat / RHO);

      fDivLat = max(fDelLon * fKorr / nDelX, fDelLat / nDelY) * 1.5;
      fDivLon = fDivLat / fKorr;

      ViewAirspace(hDC, dwIndex, dwTreeViewSel - dwCont, TRUE, PEN_SIZE_NORMAL);

      break;
    }

    case OA_TYP_CIRCLE:
    case OA_TYP_ARC:
    {
      DWORD dwElem = sCont[dwTreeViewSel].dwIndex;
      DWORD dwCont = sCircles[dwElem].dwAirspace;
      DWORD dwIndex = sCont[dwCont].dwIndex;

      float fDelLat = sAirspaces[dwIndex].fLatMax - sAirspaces[dwIndex].fLatMin;
      float fDelLon = sAirspaces[dwIndex].fLonMax - sAirspaces[dwIndex].fLonMin;

      fMidLat = sAirspaces[dwIndex].fLatMin + fDelLat / 2.0;
      fMidLon = sAirspaces[dwIndex].fLonMin + fDelLon / 2.0;

      float fKorr = cos(fMidLat / RHO);

      fDivLat = max(fDelLon * fKorr / nDelX, fDelLat / nDelY) * 1.5;
      fDivLon = fDivLat / fKorr;

      ViewAirspace(hDC, dwIndex, dwTreeViewSel - dwCont, TRUE, PEN_SIZE_NORMAL);

      break;
    }
  }

  DeleteObject(hPen);

  EndPaint(hWnd, (LPPAINTSTRUCT) & pS);

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: ShowInfoItem                                                   *
 *                                                                          *
 * Aufgabe : InfoBox (ListView) füllen                                      *
 *                                                                          *
 ****************************************************************************/
void ShowInfoItem(int nItem, char *szInfo, char *szString)
{
  LV_ITEM lvItem;

  memset(&lvItem, 0, sizeof( LV_ITEM ) );

  lvItem.mask = LVIF_TEXT;

  lvItem.iItem = nItem;
  lvItem.pszText = szInfo;

  ListView_InsertItem(hWndLV, &lvItem);
  ListView_SetItemText(hWndLV, nItem, 1, szString);

  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: ShowInfo                                                       *
 *                                                                          *
 * Aufgabe : Zeigt die Infos eines Luftraums abhängig vom Typ in der        *
 *           Box unten links                                                *
 *                                                                          *
 ****************************************************************************/
void ShowInfo(BYTE bTyp, DWORD dwIndex)
{
  char szBuffer[30];

  switch (bTyp)
  {
    case OA_TYP_FILE:
    {
      DWORD dwI, dwCount = 0;
      for (dwI = 0; dwI < dwAirspaceZ; dwI++)                                 // die gültigen Lufträume zählen 
      {
        if (TRUE == sAirspaces[dwI].bValid)
        {
          dwCount++;
        }
      }
      sprintf(szBuffer, "%lu", dwCount);                                      // ..und ausgeben
      ShowInfoItem(0, "Lufträume", szBuffer);

      return;
    }

    case OA_TYP_AIRSPACE:  // Luftraum
    {
      BYTE bIdx = 0;

      ShowInfoItem(bIdx++, "Name", pStrings + sAirspaces[dwIndex].dwName);

      if (sAirspaces[dwIndex].dwClass)
      {
        ShowInfoItem(bIdx++, "Klasse", pStrings + sAirspaces[dwIndex].dwClass);
      }

      if (sAirspaces[dwIndex].dwHigh)
      {
        ShowInfoItem(bIdx++, "Obergrenze", pStrings + sAirspaces[dwIndex].dwHigh);
      }

      if (sAirspaces[dwIndex].dwLow)
      {
        ShowInfoItem(bIdx++, "Untergrenze", pStrings + sAirspaces[dwIndex].dwLow);
      }

      sprintf(szBuffer, "%lu", sAirspaces[dwIndex].dwElemAnz);
      ShowInfoItem(bIdx++, "Elemente", szBuffer);

      return;
    }

    case OA_TYP_POINT: // Punkt
    {
      ShowInfoItem(0, "Typ", "Punkt");

      sprintf(szBuffer, "%7.4f", sPoints[dwIndex].fLat);
      ShowInfoItem(1, "Latitude", szBuffer);

      sprintf(szBuffer, "%7.4f", sPoints[dwIndex].fLon);
      ShowInfoItem(2, "Longitude", szBuffer);
      return;
    }

    case OA_TYP_ARC: // Kreisbogen
    {
      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fRad * 60.0);

      if (OA_ROT_DIR_LEFT == sCircles[dwIndex].bRotDir)
      {
        strcat(szBuffer, " links");
      }
      else
      {
        strcat(szBuffer, " rechts");
      }
      strcat(szBuffer, "drehend");
      ShowInfoItem(0, "Radius", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLatM);
      ShowInfoItem(1, "Mittelpunkt-Lat", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLonM);
      ShowInfoItem(2, "Mittelpunkt-Lon", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLatB);
      ShowInfoItem(3, "Startpunkt-Lat", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLonB);
      ShowInfoItem(4, "Startpunkt-Lon", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLatE);
      ShowInfoItem(5, "Endpunkt-Lat", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLonE);
      ShowInfoItem(6, "Endpunkt-Lon", szBuffer);

      return;
    }

    case OA_TYP_CIRCLE:  // Kreis
    {
      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fRad * 60.0);
      ShowInfoItem(0, "Radius", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLatM);
      ShowInfoItem(1, "Latitude", szBuffer);

      sprintf(szBuffer, "%7.4f", sCircles[dwIndex].fLonM);
      ShowInfoItem(2, "Longitude", szBuffer);

      return;
    }
  }
  return;
}

/****************************************************************************
 *                                                                          *
 * Programm: WndProc                                                        *
 *                                                                          *
 * Aufgabe : Eventbehandlungsroutine für das MainFenster                    *
 *                                                                          *
 ****************************************************************************/
LRESULT WINAPI WndProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
  LV_COLUMN lvc;

  switch (wMsg)
  {
    case WM_COMMAND:

      switch (wParam)
      {
        case IDM_OPEN:
        {
          OPENFILENAME ofn;
          szFile[0] = 0;

          memset(&ofn, 0, sizeof(ofn));

          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hWnd;
          ofn.hInstance = ghInstance;
          ofn.lpstrFilter = "OpenAir Datei (*.txt)\0*.txt\0Flytec OpenAir Datei (*.faf)\0*.faf\0";
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = MAX_PATH;
          ofn.lpstrTitle = "OpenAir Datei öffnen";
          ofn.Flags = OFN_EXPLORER;
          ofn.lpstrDefExt = "txt";

          if (GetOpenFileName(&ofn))
          {
            ReadOAFile(szFile);
            BuildTreeView();
          }

          return TRUE;
        }

        case IDM_SAVE:
        {
          OPENFILENAME ofn;

          memset(&ofn, 0, sizeof(ofn));

          ofn.lStructSize = sizeof(ofn);
          ofn.hwndOwner = hWnd;
          ofn.hInstance = ghInstance;
          ofn.lpstrFilter = "OpenAir Datei (*.txt)\0*.txt\0Flytec OpenAir Datei (*.faf)\0*.faf\0";
          ofn.lpstrFile = szFile;
          ofn.nMaxFile = MAX_PATH;
          ofn.lpstrTitle = "OpenAir Datei speichern";
          ofn.Flags = OFN_EXPLORER;
          ofn.lpstrDefExt = "txt";

          if (GetSaveFileName(&ofn))
          {
            SaveOAFile(szFile);
            //item.pszText = strrchr(szFile, '\\') + 1;
          }  

          return TRUE;
        }

        case IDM_SELECT:
        {
          DialogBox(ghInstance, MAKEINTRESOURCE(IDD_SELECT), hWnd, (DLGPROC)SelectDlgProc);
          BuildTreeView();                                                    // Baumstruktur aufbauen
          ListView_DeleteAllItems(hWndLV);                                    // InfoBox leeren
          ShowInfo(sCont[0].bTyp, sCont[0].dwIndex);                          // InfoBox füllen       
          InvalidateRect(hWnd, &sRectView, TRUE);                             // Zeichenfläche ungültig machen (führt zum Neuzeichnen) 

          return TRUE;
        }

        case IDM_HELP:

          DialogBox(ghInstance, MAKEINTRESOURCE(IDD_HELP), hWnd, (DLGPROC)HelpDlgProc);

          return TRUE;

        case IDM_ENDE:
        case IDOK:

          PostMessage(hWnd, WM_CLOSE, 0, 0L);

          return (0);
      }
      break;

    case WM_SIZE:

      MoveWindow(hWndTV, 0, 0, IBOXB - 1, HIWORD(lParam) - IBOXH - 20, TRUE);
      MoveWindow(hWndLV, 0, HIWORD(lParam) - IBOXH - 20, IBOXB - 1, IBOXH - 1, TRUE);
      MoveWindow(hWndSB, 0, HIWORD(lParam) - 20, LOWORD(lParam), HIWORD(lParam), TRUE);

      GetClientRect(hWnd, &sRectView);

      sRectView.bottom -= 20;
      sRectView.left += IBOXB;

      InvalidateRect(hWnd, &sRectView, TRUE);

      return 0;

    case WM_PAINT:

      View(hWnd);

      return 0;

    case WM_DROPFILES:
    {
      DragQueryFile((HDROP)wParam, 0, szFile, MAX_PATH);  // Filenamen extrahieren
      DragFinish((HDROP)wParam);  // Fertig
      ReadOAFile(szFile); // Datei lesen

      return 0;
    }

    case WM_NOTIFY:
    {
      LPNMHDR lpNmh = (LPNMHDR)lParam;

      if (lpNmh->hwndFrom == hWndTV && lpNmh->code == TVN_SELCHANGED)
      {
        HTREEITEM hItem = TreeView_GetSelection(hWndTV);
        TV_ITEM tvItem;

        ListView_DeleteAllItems(hWndLV);

        memset(&tvItem, 0, sizeof(TV_ITEM));

        tvItem.hItem = hItem;
        tvItem.mask = TVIF_PARAM;

        TreeView_GetItem(hWndTV, &tvItem);

        dwTreeViewSel = tvItem.lParam;

        ShowInfo(sCont[dwTreeViewSel].bTyp, sCont[dwTreeViewSel].dwIndex);

        InvalidateRect(hWnd, &sRectView, TRUE);
      }
      return 0;
    }

    case WM_CREATE:

      InitCommonControls();

      DragAcceptFiles(hWnd, TRUE);

      hWndTV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS, 0, 0, 0, 0, hWnd, 0, ghInstance, NULL);

      // TreeImages setzen
      HIMAGELIST hImages = ImageList_LoadBitmap(ghInstance, MAKEINTRESOURCE(IDC_TREE), 16, 2, CLR_DEFAULT);
      
      if (hImages)
      {
        TreeView_SetImageList(hWndTV, hImages, TVSIL_NORMAL);
      }
      
      hWndLV = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT, 0, 0, 0, 0, hWnd, 0, ghInstance, NULL);

      ZeroMemory(&lvc, sizeof(lvc));

      lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
      lvc.cx = IBOXS1;
      lvc.pszText = "Info";

      ListView_InsertColumn(hWndLV, 0, &lvc);

      lvc.cx = IBOXS2;
      lvc.pszText = "Inhalt";

      ListView_InsertColumn(hWndLV, 1, &lvc);

      hWndSB = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER | SBARS_SIZEGRIP, "OK", hWnd, IDC_STBA);

      return 0;

    case WM_DESTROY:

      DragAcceptFiles(hWnd, FALSE);
      PostQuitMessage(0);

      return 0;

    default:

      return DefWindowProc(hWnd, wMsg, wParam, lParam);
  }
  return 0;
}
