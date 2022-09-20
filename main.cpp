#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>

#include <list>
#include <memory>
using namespace std;

#pragma comment(lib, "d2d1")

#include "basewin.h"
#include "resource.h"

//button format macros
#define BUTTONWIDTH 310
#define BUTTONHEIGHT 100
#define BUTTONXCOOR 20
#define BUTTONYCOOR 20
#define BUTTONPADDING 10

//button control IDs
#define MINKOWSKI_DIFFERENCE 100
#define MINKOWSKI_SUM 101
#define QUICK_HULL 102
#define POINT_CONVEX_HULL_INTERSECTION 103
#define GJK 104
#define EXIT 105

//drawing format macros
#define VERTEX_RADIUS 10.0
#define MIN_VERTEX_X 300
#define MAX_VERTEX_X 700
#define MIN_VERTEX_Y 100
#define MAX_VERTEX_Y 500


template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class DPIScale
{
    static float scaleX;
    static float scaleY;

public:
    static void Initialize(ID2D1Factory *pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX/96.0f;
        scaleY = dpiY/96.0f;
    }

    template <typename T>
    static float PixelsToDipsX(T x)
    {
        return static_cast<float>(x) / scaleX;
    }

    template <typename T>
    static float PixelsToDipsY(T y)
    {
        return static_cast<float>(y) / scaleY;
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct MyVertex
{
    D2D1_POINT_2F vertex;

};

struct MyEllipse
{
    D2D1_ELLIPSE          ellipse;
    D2D1_COLOR_F          color;
    shared_ptr<MyVertex>  vertex;

    void Draw(ID2D1RenderTarget *pRT, ID2D1SolidColorBrush *pBrush)
    {
        pBrush->SetColor(color);
        pRT->FillEllipse(ellipse, pBrush);
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
        pRT->DrawEllipse(ellipse, pBrush, 1.0f);
    }

    BOOL HitTest(float x, float y)
    {
        const float a = ellipse.radiusX;
        const float b = ellipse.radiusY;
        const float x1 = x - ellipse.point.x;
        const float y1 = y - ellipse.point.y;
        const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
        return d <= 1.0f;
    }

    shared_ptr<MyVertex> GetVertexPointer()
    {
        return vertex;
    }
};



D2D1::ColorF::Enum colors[] = { D2D1::ColorF::Yellow, D2D1::ColorF::Salmon, D2D1::ColorF::LimeGreen };


class MainWindow : public BaseWindow<MainWindow>
{
    enum Mode
    {
        DrawMode,
        SelectMode,
        DragMode
    };

    //enum class to keep track of which algorithm is being demonstrated
    enum class AlgoMode
    {
        MinkowskiDifference,
        MinkowskiSum,
        QuickHull,
        PointConvexHullIntersection,
        gjk
    };

    HCURSOR                 hCursor;

    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRenderTarget;
    ID2D1SolidColorBrush    *pBrush;
    D2D1_POINT_2F           ptMouse;

    Mode                    mode;
    AlgoMode                algoMode;
    size_t                  nextColor;

    list<shared_ptr<MyEllipse>>             ellipses;
    list<shared_ptr<MyEllipse>>             ellipses2;
    list<shared_ptr<MyVertex>>             convexHull;
    list<shared_ptr<MyVertex>>             convexHull2;

    list<shared_ptr<MyEllipse>>::iterator   selection;

    BOOL selection1;


    //vertices of the convex hulls
    list<MyVertex> vertices1;
    list<MyVertex> vertices2;

     
    shared_ptr<MyEllipse> Selection() 
    { 

        if(selection1 && selection == ellipses.end())
        { 
            return nullptr;
        }
        if(!selection1 && selection == ellipses2.end())
        {
            return nullptr;
        }
            return (*selection);
    }

    void    ClearSelection() { selection = ellipses.end(); }
    HRESULT InsertEllipse(float x, float y);
    std::shared_ptr<MyEllipse> GenerateRandomEllipse(D2D1::ColorF color);
    void generateRandomSetOfPoints(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2);

    BOOL    HitTest(float x, float y);
    void    SetMode(Mode m);
    void    MoveSelection(float x, float y);
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    Resize();
    void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void    OnLButtonUp();
    void    OnMouseMove(int pixelX, int pixelY, DWORD flags);
    void    OnKeyDown(UINT vkey);

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL), 
        ptMouse(D2D1::Point2F()), nextColor(0), selection(ellipses.end())
    {
    }

    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
        }
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
     
        pRenderTarget->BeginDraw();

        pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::SkyBlue) );


        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        for (auto i = ellipses2.begin(); i != ellipses2.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        if (Selection())
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
            pRenderTarget->DrawEllipse(Selection()->ellipse, pBrush, 2.0f);
        }

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);

        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    //if (mode == DrawMode)
    //{
    //    POINT pt = { pixelX, pixelY };

    //    if (DragDetect(m_hwnd, pt))
    //    {
    //        SetCapture(m_hwnd);
    //    
    //        // Start a new ellipse.
    //        InsertEllipse(dipX, dipY);
    //    }
    //}
    //else
    //{
        ClearSelection();

        if (HitTest(dipX, dipY))
        {
            SetCapture(m_hwnd);

            ptMouse = Selection()->ellipse.point;
            ptMouse.x -= dipX;
            ptMouse.y -= dipY;
            Selection()->vertex->vertex.x -= dipX;
            Selection()->vertex->vertex.x -= dipY;

            SetMode(DragMode);
        }
    //}
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonUp()
{
    if ((mode == DrawMode) && Selection())
    {
        ClearSelection();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
    else if (mode == DragMode)
    {
        SetMode(SelectMode);
    }
    ReleaseCapture(); 
}


void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    if ((flags & MK_LBUTTON) && Selection())
    { 
        if (mode == DrawMode)
        {
            // Resize the ellipse.
            const float width = (dipX - ptMouse.x) / 2;
            const float height = (dipY - ptMouse.y) / 2;
            const float x1 = ptMouse.x + width;
            const float y1 = ptMouse.y + height;

            Selection()->ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
        }
        else if (mode == DragMode)
        {
            // Move the ellipse.
            Selection()->ellipse.point.x = dipX + ptMouse.x;
            Selection()->ellipse.point.y = dipY + ptMouse.y;
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnKeyDown(UINT vkey)
{
    switch (vkey)
    {
    case VK_BACK:
    case VK_DELETE:
        if ((mode == SelectMode) && Selection())
        {
            ellipses.erase(selection);
            ClearSelection();
            SetMode(SelectMode);
            InvalidateRect(m_hwnd, NULL, FALSE);
        };
        break;

    case VK_LEFT:
        MoveSelection(-1, 0);
        break;

    case VK_RIGHT:
        MoveSelection(1, 0);
        break;

    case VK_UP:
        MoveSelection(0, -1);
        break;

    case VK_DOWN:
        MoveSelection(0, 1);
        break;
    }
}

HRESULT MainWindow::InsertEllipse(float x, float y)
{
    try
    {
        selection = ellipses.insert(
            ellipses.end(), 
            shared_ptr<MyEllipse>(new MyEllipse()));

        Selection()->ellipse.point = ptMouse = D2D1::Point2F(x, y);
        Selection()->ellipse.radiusX = Selection()->ellipse.radiusY = 2.0f; 
        Selection()->color = D2D1::ColorF( colors[nextColor] );

        nextColor = (nextColor + 1) % ARRAYSIZE(colors);
    }
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

std::shared_ptr<MyEllipse> MainWindow::GenerateRandomEllipse(D2D1::ColorF color)
{
   
    std::shared_ptr<MyEllipse> newEllipse = std::make_shared<MyEllipse>();
    newEllipse->ellipse = D2D1::Ellipse(D2D1::Point2F(rand() % MAX_VERTEX_X + MIN_VERTEX_Y, rand() % MAX_VERTEX_X + MIN_VERTEX_Y), VERTEX_RADIUS, VERTEX_RADIUS);
    newEllipse->color = (color);
    newEllipse->vertex = std::make_shared<MyVertex>();
    newEllipse->vertex->vertex = newEllipse->ellipse.point;

    return newEllipse;
}

void MainWindow::generateRandomSetOfPoints(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2)
{
    for (size_t i = 0; i < num1; i++)
    {
        ellipses.emplace_back(GenerateRandomEllipse(color1));
    }
    for (size_t i = 0; i < num2; i++)
    {
        ellipses2.emplace_back(GenerateRandomEllipse(color2));
    }
}


BOOL MainWindow::HitTest(float x, float y)
{
 
    for (auto i = ellipses.rbegin(); i != ellipses.rend(); ++i)
    {
        if ((*i)->HitTest(x, y))
        {
            selection = (++i).base();
            selection1 = true;
            return TRUE;
        }
    }
    for (auto j = ellipses2.rbegin(); j != ellipses2.rend(); ++j)
    {
        if ((*j)->HitTest(x, y))
        {
            selection = (++j).base();
            selection1 = false;
            return TRUE;
        }
    }
    return FALSE;
}

void MainWindow::MoveSelection(float x, float y)
{
    if ((mode == SelectMode) && Selection())
    {
        Selection()->ellipse.point.x += x;
        Selection()->ellipse.point.y += y;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::SetMode(Mode m)
{
    mode = m;

    LPWSTR cursor;
    switch (mode)
    {
    case DrawMode:
        cursor = IDC_CROSS;
        break;

    case SelectMode:
        cursor = IDC_HAND;
        break;

    case DragMode:
        cursor = IDC_SIZEALL;
        break;
    }

    hCursor = LoadCursor(NULL, cursor);
    SetCursor(hCursor);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Draw Circles", WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));

    /*--------------------------------------------------------------------
        Instantiate all the buttons that this app will use.
        the first five button will change with algorithm the user will
        interface with. These buttons will change the enum value for the
        main switch case, which change what the main window draws and will
        change how the application takes in user input
        The last button will simply close out the application
    --------------------------------------------------------------------*/
    HWND mdButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"MINKOWSKI DIFFERENCE",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR,         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)MINKOWSKI_DIFFERENCE,       
        hInstance,
        NULL);      // Pointer not needed.

    HWND msButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"MINKOWSKI SUM",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR+(BUTTONHEIGHT+BUTTONPADDING),         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)MINKOWSKI_SUM,
        hInstance,
        NULL);      // Pointer not needed.

    HWND qhButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"QUICK HULL",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING)*2,         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)QUICK_HULL,
        hInstance,
        NULL);      // Pointer not needed.


    HWND pchiButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"POINT CONVEX HULL INTERSECTION",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING) * 3,         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)POINT_CONVEX_HULL_INTERSECTION,
        hInstance,
        NULL);      // Pointer not needed.

    HWND gjkButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"GJK",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING) * 4,         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)GJK,
        hInstance,
        NULL);      // Pointer not needed.

    HWND eButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"EXIT APPLICATION",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        BUTTONXCOOR,         // x position 
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING) * 5,         // y position 
        BUTTONWIDTH,        // Button width
        BUTTONHEIGHT,        // Button height
        win.Window(),     // Parent window
        (HMENU)EXIT,
        hInstance,
        NULL);      // Pointer not needed.

    ShowWindow(win.Window(), nCmdShow);

    ShowWindow(mdButton, SW_SHOW);
    UpdateWindow(mdButton);

    ShowWindow(msButton, SW_SHOW);
    UpdateWindow(msButton);

    ShowWindow(qhButton, SW_SHOW);
    UpdateWindow(qhButton);

    ShowWindow(pchiButton, SW_SHOW);
    UpdateWindow(pchiButton);

    ShowWindow(gjkButton, SW_SHOW);
    UpdateWindow(gjkButton);

    ShowWindow(eButton, SW_SHOW);
    UpdateWindow(eButton);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(win.Window(), hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
        DPIScale::Initialize(pFactory);
        SetMode(DragMode);
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_SIZE:
        Resize();
        return 0;

    case WM_LBUTTONDOWN: 
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_LBUTTONUP: 
        OnLButtonUp();
        return 0;

    case WM_MOUSEMOVE: 
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(hCursor);
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        OnKeyDown((UINT)wParam);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_DRAW_MODE:
            SetMode(DrawMode);
            break;

        case ID_SELECT_MODE:
            SetMode(SelectMode);
            break;

        case ID_TOGGLE_MODE:
            if (mode == DrawMode)
            {
                SetMode(SelectMode);
            }
            else
            {
                SetMode(DrawMode);
            }
            break;

        }

    //Handle Button Inputs to change the state of the application or close the application
    case BN_CLICKED:
        if (LOWORD(wParam) == MINKOWSKI_DIFFERENCE)
        {
            OutputDebugStringW(L"MINKOWSKI_DIFFERENCE\n");
            algoMode = AlgoMode::MinkowskiDifference;
            break;
        }
        if (LOWORD(wParam) == MINKOWSKI_SUM)
        {
            OutputDebugStringW(L"MINKOWSKI_SUM\n");
            algoMode = AlgoMode::MinkowskiSum;
            break;
        }
        if (LOWORD(wParam) == QUICK_HULL)
        {
            OutputDebugStringW(L"QUICK_HULL\n");
            algoMode = AlgoMode::QuickHull;
            break;
        }
        if (LOWORD(wParam) == POINT_CONVEX_HULL_INTERSECTION)
        {
            OutputDebugStringW(L"POINT_CONVEX_HULL_INTERSECTION\n");
            algoMode = AlgoMode::PointConvexHullIntersection;
            generateRandomSetOfPoints(1,10, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Yellow));
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == GJK)
        {
            OutputDebugStringW(L"GJK\n");
            algoMode = AlgoMode::gjk;
            break;
        }
        if (LOWORD(wParam) == EXIT)
        {
           OutputDebugStringW(L"EXIT\n");
           DiscardGraphicsResources();
           SafeRelease(&pFactory);
           PostQuitMessage(0);
           return 0;
        }
        else
            break;
        
        return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}
