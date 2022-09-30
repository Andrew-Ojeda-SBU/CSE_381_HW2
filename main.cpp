#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>

#include <list>
#include <memory>
#include <vector>
using namespace std; 

#pragma comment(lib, "d2d1")

#include "basewin.h"
#include "resource.h"
#include <functional>

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
#define MAX_VERTEX_X 400
#define MIN_VERTEX_Y 100
#define MAX_VERTEX_Y 300

//button area Macros
#define BUTTON_AREA_WIDTH 350


template <class T> void SafeRelease(T** ppT)
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
    static void Initialize(ID2D1Factory* pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX / 96.0f;
        scaleY = dpiY / 96.0f;
    }

    template <typename T>
    static float PixelsToDipsX(T x)
    {
        return static_cast<float>(x) / scaleX - BUTTON_AREA_WIDTH/scaleX;
    }

    template <typename T>
    static float PixelsToDipsY(T y)
    {
        return static_cast<float>(y) / scaleY;
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

//struct D2D_POINT_2F
//{
//    D2D1_POINT_2F vertex;
//};


struct MyEllipse
{
    D2D1_ELLIPSE          ellipse;
    D2D1_COLOR_F          color;
    /*shared_ptr<D2D_POINT_2F>  vertex;*/

    void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush)
    {
        pBrush->SetColor(color);
        pRT->FillEllipse(ellipse, pBrush);
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::FloralWhite));
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

    shared_ptr<D2D_POINT_2F> GetVertexPointer()
    {
        return make_shared<D2D1_POINT_2F>(ellipse.point);
    }
};



D2D1::ColorF::Enum colors[] = { D2D1::ColorF::Yellow, D2D1::ColorF::Salmon, D2D1::ColorF::LimeGreen };


class MainWindow : public BaseWindow<MainWindow>
{
    enum Mode
    {
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

    ID2D1Factory* pFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    ID2D1SolidColorBrush* pBrush;
    D2D1_POINT_2F           ptMouse;

    Mode                    mode;
    AlgoMode                algoMode;
    size_t                  nextColor;

    list<shared_ptr<MyEllipse>>             ellipses;
    list<shared_ptr<MyEllipse>>             ellipses2;
    list<shared_ptr<MyEllipse>>             ellipses3;
    vector<D2D1_POINT_2F>                   prevPoints;
    vector<shared_ptr<D2D1_POINT_2F>>       convexHull;
    vector<shared_ptr<D2D1_POINT_2F>>       convexHull2;
    vector<shared_ptr<D2D1_POINT_2F>>       convexHull3;//used for MinkowskiSum, MinkowskiDiff, and GJK
    shared_ptr<MyEllipse>                   graphOrigin; //used for MinkowskiSum, MinkowskiDiff, and GJK

    list<shared_ptr<MyEllipse>>::iterator   selection;
    list<shared_ptr<MyEllipse>>::iterator   selection2;

    BOOL selection1;
    BOOL noSelection;
    BOOL convexHullDrag = FALSE;

    FLOAT zoomScale = 1;
    FLOAT minZoomScale = 0.6f;
    FLOAT maxZoomScale = 8;

    shared_ptr<MyEllipse> Selection()
    {

        if (selection1 && selection == ellipses.end())
        {
            return nullptr;
        }
        if (!selection1 && selection2 == ellipses2.end())
        {
            return nullptr;
        }
        return selection1 ? (*selection) : (*selection2);
    }

    void    ClearSelection() { selection = ellipses.end(); selection2 = ellipses2.end(); }
    void    ResetZoom();
    HRESULT InsertEllipse(float x, float y);
    std::shared_ptr<MyEllipse> GenerateRandomEllipse(D2D1::ColorF color, int maxX, int maxY, int minX, int minY);
    void GenerateRandomSetOfPoints(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2);
    void GenerateRandomSetOfPointsOnGrid(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2);
    void GenerateOrigin();


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
    void    ScalePoints(float scale);

    void ClearLists();
    void AlgoTest();
    BOOL IsRight(shared_ptr<D2D_POINT_2F> a, shared_ptr<D2D_POINT_2F> b, shared_ptr<D2D_POINT_2F> c);
    void QuickHull(const list<shared_ptr<MyEllipse>>& points, vector<shared_ptr<D2D_POINT_2F>>& convexHull);
    BOOL PointInConvexHull(shared_ptr<D2D_POINT_2F> point, const vector<shared_ptr<D2D_POINT_2F>>& convexHull);
    void MinkowskiSum(const vector<shared_ptr<D2D_POINT_2F>>& convexHull,const  vector<shared_ptr<D2D_POINT_2F>>& convexHull2, list<shared_ptr<MyEllipse>>& points);
    void MinkowskiDiff(const vector<shared_ptr<D2D_POINT_2F>>& convexHull,const vector<shared_ptr<D2D_POINT_2F>>& convexHull2, list<shared_ptr<MyEllipse>>& points);

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
        
        HWND renderArea = GetWindow(m_hwnd, GW_CHILD);

        RECT rc;
        GetClientRect(renderArea, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(renderArea, size),
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
        BeginPaint(GetWindow(m_hwnd, GW_CHILD), &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        if (algoMode == AlgoMode::MinkowskiSum || algoMode == AlgoMode::MinkowskiDifference || algoMode == AlgoMode::gjk)
        {
            RECT rc;
            GetClientRect(GetWindow(m_hwnd, GW_CHILD), &rc);
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::DimGray));
            float top = (float)rc.top;
            float bottom = (float)rc.bottom;
            float left = (float)rc.left;
            float right = (float)rc.right;
            // For the algorithms that are displayed with the coordinate grid, draw the grid
            // First, draw the thicher lines for the x and y axis
            pRenderTarget->DrawLine(D2D1::Point2F(graphOrigin->ellipse.point.x, top), D2D1::Point2F(graphOrigin->ellipse.point.x, bottom), pBrush, 3.5f);
            pRenderTarget->DrawLine(D2D1::Point2F(left, graphOrigin->ellipse.point.y), D2D1::Point2F(right, graphOrigin->ellipse.point.y), pBrush, 3.5f);
            // Now, draw the rest of the grid lines on screen
            // Vertical
            float lineGap = 20 * zoomScale;
            for (float i = graphOrigin->ellipse.point.x + lineGap; i < right; i = i + lineGap)
            {
                pRenderTarget->DrawLine(D2D1::Point2F(i, top), D2D1::Point2F(i, bottom), pBrush, 0.5f);
            }
            for (float i = graphOrigin->ellipse.point.x - lineGap; i > left; i = i - lineGap)
            {
                pRenderTarget->DrawLine(D2D1::Point2F(i, top), D2D1::Point2F(i, bottom), pBrush, 0.5f);
            }
            // Horizontal
            for (float i = graphOrigin->ellipse.point.y + lineGap; i < bottom; i = i + lineGap)
            {
                pRenderTarget->DrawLine(D2D1::Point2F(left, i), D2D1::Point2F(right, i), pBrush, 0.5f);
            }
            for (float i = graphOrigin->ellipse.point.y - lineGap; i > top; i = i - lineGap)
            {
                pRenderTarget->DrawLine(D2D1::Point2F(left, i), D2D1::Point2F(right, i), pBrush, 0.5f);
            }



            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::FloralWhite));
        }

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->ellipse.radiusX = (float)(VERTEX_RADIUS * zoomScale);
            (*i)->ellipse.radiusY = (float)(VERTEX_RADIUS * zoomScale);
            (*i)->Draw(pRenderTarget, pBrush);
        }

        if (algoMode != AlgoMode::PointConvexHullIntersection)
        {
            for (auto i = ellipses2.begin(); i != ellipses2.end(); ++i)
            {
                (*i)->ellipse.radiusX = (float)(VERTEX_RADIUS * zoomScale);
                (*i)->ellipse.radiusY = (float)(VERTEX_RADIUS * zoomScale);
                (*i)->Draw(pRenderTarget, pBrush);
            }
        }

        // Draw hulls 1 and 2 in red and blue in gjk, like on the website
        if (algoMode == AlgoMode::gjk)
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
        }
        for (auto i = convexHull.begin(), prev = convexHull.end();
            i != convexHull.end(); prev = i, ++i)
        {
            if (prev != convexHull.end())
                pRenderTarget->DrawLine(*(*i), *(*prev), pBrush, 1.5f);
        }
        if (!convexHull.empty())
            pRenderTarget->DrawLine(*convexHull.front(), *convexHull.back(), pBrush, 1.5f);


        if (algoMode == AlgoMode::gjk)
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Blue));
        }
        for (auto i = convexHull2.begin(), prev = convexHull2.end();
            i != convexHull2.end(); prev = i, ++i)
        {
            if (prev != convexHull2.end())
                pRenderTarget->DrawLine(*(*i), *(*prev), pBrush, 1.5f);
        }
        if (!convexHull2.empty())
            pRenderTarget->DrawLine(*convexHull2.front(), *convexHull2.back(), pBrush, 1.5f);

        // Set the lines of convexHull3 to be pink like on the website
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Magenta));
        // If the hulls are colliding in gjk, make convexHull3 green
        if (algoMode == AlgoMode::gjk)
        {
            if (PointInConvexHull(graphOrigin->GetVertexPointer(), convexHull3))
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::LawnGreen));
        }
        for (auto i = convexHull3.begin(), prev = convexHull3.end();
            i != convexHull3.end(); prev = i, ++i)
        {
            if (prev != convexHull3.end())
                pRenderTarget->DrawLine(*(*i), *(*prev), pBrush, 1.5f);
        }
        if (!convexHull3.empty())
            pRenderTarget->DrawLine(*convexHull3.front(), *convexHull3.back(), pBrush, 1.5f);
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::FloralWhite));

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

        HWND renderArea = GetWindow(m_hwnd, GW_CHILD);

        MoveWindow(renderArea,rc.left+BUTTON_AREA_WIDTH,rc.top,rc.right,rc.bottom,TRUE);
        /*RECT rc;*/
        GetClientRect(renderArea, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);

        InvalidateRect(renderArea, NULL, FALSE);

    }
}

void MainWindow::ResetZoom()
{
    zoomScale = 1;
}

void MainWindow::ScalePoints(float scale)
{

    for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
    {
        (*i)->ellipse.point.x = graphOrigin->ellipse.point.x + (((*i)->ellipse.point.x - graphOrigin->ellipse.point.x) * scale);
        (*i)->ellipse.point.y = graphOrigin->ellipse.point.y + (((*i)->ellipse.point.y - graphOrigin->ellipse.point.y) * scale);
    }

    for (auto i = ellipses2.begin(); i != ellipses2.end(); ++i)
    {
        (*i)->ellipse.point.x = graphOrigin->ellipse.point.x + (((*i)->ellipse.point.x - graphOrigin->ellipse.point.x) * scale);
        (*i)->ellipse.point.y = graphOrigin->ellipse.point.y + (((*i)->ellipse.point.y - graphOrigin->ellipse.point.y) * scale);
    }
    convexHull.clear();
    convexHull2.clear();
    convexHull3.clear();
    ellipses3.clear();
    AlgoTest();
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);
    D2D1_POINT_2F mousePoint;
    mousePoint.x = dipX;
    mousePoint.y = dipY;

    ClearSelection();

    if (HitTest(dipX, dipY))
    {
        SetCapture(m_hwnd);

        convexHull.clear();
        convexHull2.clear();
        convexHull3.clear();
        ptMouse = Selection()->ellipse.point;
        ptMouse.x -= dipX;
        ptMouse.y -= dipY;
        /*Selection()->vertex->x -= dipX;
        Selection()->vertex->x -= dipY;*/
        AlgoTest();
        /*   OnPaint();*/
        convexHullDrag = FALSE;
        SetMode(DragMode);
    }
    else
        if (convexHull.size() > 3 && PointInConvexHull(make_shared<D2D1_POINT_2F>(mousePoint), convexHull))
        {
            OutputDebugStringW(L"convexhull 1 drag\n");
            prevPoints.clear();
            for (auto point : ellipses)
            {
                prevPoints.emplace_back(D2D1::Point2F(point->ellipse.point.x - dipX, point->ellipse.point.y - dipY));
            }
            selection1 = TRUE;
            convexHullDrag = TRUE;
            noSelection = FALSE;
            SetMode(DragMode);
        }
        else
            if (convexHull2.size() > 3 && PointInConvexHull(make_shared<D2D1_POINT_2F>(mousePoint), convexHull2))
            {
                OutputDebugStringW(L"convexhull 2 drag\n");
                prevPoints.clear();
                for (auto const& point : ellipses2)
                {
                    prevPoints.emplace_back(D2D1::Point2F(point->ellipse.point.x - dipX, point->ellipse.point.y - dipY));
                }
                selection1 = FALSE;
                convexHullDrag = TRUE;
                noSelection = FALSE;
                SetMode(DragMode);
            }
            else
                if (algoMode == AlgoMode::MinkowskiSum || algoMode == AlgoMode::MinkowskiDifference || algoMode == AlgoMode::gjk)
                {
                    if (!PointInConvexHull(make_shared<D2D1_POINT_2F>(mousePoint), convexHull) && !PointInConvexHull(make_shared<D2D1_POINT_2F>(mousePoint), convexHull2))
                    {
                        prevPoints.clear();
                        for (auto const& point : ellipses2)
                        {
                            prevPoints.emplace_back(D2D1::Point2F(point->ellipse.point.x - dipX, point->ellipse.point.y - dipY));
                        }
                        for (auto const& point : ellipses)
                        {
                            prevPoints.emplace_back(D2D1::Point2F(point->ellipse.point.x - dipX, point->ellipse.point.y - dipY));
                        }
                        prevPoints.emplace_back(D2D1::Point2F(graphOrigin->ellipse.point.x - dipX, graphOrigin->ellipse.point.y - dipY));

                        selection1 = TRUE;
                        convexHullDrag = TRUE;
                        noSelection = TRUE;
                        SetMode(DragMode);
                    }
                }

    //}
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonUp()
{
    if (mode == DragMode)
    {
        SetMode(SelectMode);
    }
    ReleaseCapture();
}


void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    if ((flags & MK_LBUTTON) && (Selection() || convexHullDrag))
    {
        if (mode == DragMode)
        {
            convexHull.clear();
            convexHull2.clear();
            convexHull3.clear();
            ellipses3.clear();

            if (convexHullDrag)
            {
                size_t count = 0;

                if (selection1)
                {
                    for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
                    {
                        (*i)->ellipse.point.x = dipX + prevPoints[count].x;
                        (*i)->ellipse.point.y = dipY + prevPoints[count].y;
                        count++;
                    }
                }
                else
                {
                    for (auto i = ellipses2.begin(); i != ellipses2.end(); ++i)
                    {
                        (*i)->ellipse.point.x = dipX + prevPoints[count].x;
                        (*i)->ellipse.point.y = dipY + prevPoints[count].y;
                        count++;
                    }
                }
                if (noSelection)
                {
                    for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
                    {
                        (*i)->ellipse.point.x = dipX + prevPoints[count].x;
                        (*i)->ellipse.point.y = dipY + prevPoints[count].y;
                        count++;
                    }
                    count = 0;
                    for (auto i = ellipses2.begin(); i != ellipses2.end(); ++i)
                    {
                        (*i)->ellipse.point.x = dipX + prevPoints[count].x;
                        (*i)->ellipse.point.y = dipY + prevPoints[count].y;
                        count++;
                    }
                    graphOrigin->ellipse.point.x = dipX + prevPoints[prevPoints.size() - 1].x;
                    graphOrigin->ellipse.point.y = dipY + prevPoints[prevPoints.size() - 1].y;
                }
            }
            else
            {

                // Move the ellipse.
                Selection()->ellipse.point.x = dipX + ptMouse.x;
                Selection()->ellipse.point.y = dipY + ptMouse.y;
            }
            AlgoTest();
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
        Selection()->color = D2D1::ColorF(colors[nextColor]);

        nextColor = (nextColor + 1) % ARRAYSIZE(colors);
    }
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}
/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::GenerateRandomEllipse

  Summary:  randomly generates a point in the window

  Args:     D2D1::ColorF color
                color of point

  Modifies: [].

  Returns:  shared_ptr<MyEllipse>
              smart pointer to a new point
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
std::shared_ptr<MyEllipse> MainWindow::GenerateRandomEllipse(D2D1::ColorF color, int maxX, int maxY, int minX, int minY)
{
    maxX = maxX - minX;
    maxY = maxY - minY;
    std::shared_ptr<MyEllipse> newEllipse = std::make_shared<MyEllipse>();
    newEllipse->ellipse = D2D1::Ellipse(D2D1::Point2F((float)(rand() % maxX + minX), (float)(rand() % maxY + minY)), VERTEX_RADIUS, VERTEX_RADIUS);
    newEllipse->color = (color);
    /* newEllipse->vertex = std::make_shared<D2D_POINT_2F>();
     newEllipse->vertex->vertex = newEllipse->ellipse.point;*/

    return newEllipse;
}
/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::GenerateRandomSetOfPoints

  Summary:  creates two lists of points that are randomly generated

  Args:     size_t num1
              size of ellipses
            size_t num2
              size of ellipses2
            size_t color1
              color of points in ellipses
            size_t color2
              color of points in ellipses2

  Modifies: [ellipses, ellipses2].

  Returns:  VOID
              No return type
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::GenerateRandomSetOfPoints(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2)
{
    RECT rc;
    GetClientRect(GetWindow(m_hwnd, GW_CHILD), &rc);
    int middleX = rc.right / 2;
    int middleY = rc.bottom / 2;
    for (size_t i = 0; i < num1; i++)
    {
        ellipses.emplace_back(GenerateRandomEllipse(color1, (int)(middleX * 1.2), (int)(middleY * 1.2), (int)((middleX * .5) > (350) ? (middleX * .5) : (350)), (int)(middleY * .2)));
    }
    for (size_t i = 0; i < num2; i++)
    {
        ellipses2.emplace_back(GenerateRandomEllipse(color2, (int)(middleX * 1.4), (int)(middleY * 1.4), (int)((middleX * .4) > (350) ? (middleX * .4) : (350)), (int)(middleY * .35)));
    }
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::GenerateRandomSetOfPointsOnGrid

  Summary:  same as GenerateRandomSetOfPoints, except the points appear in the appropriate positions on the grid for the minkowski algorithms and gjk

  Args:     size_t num1
              size of ellipses
            size_t num2
              size of ellipses2
            size_t color1
              color of points in ellipses
            size_t color2
              color of points in ellipses2

  Modifies: [ellipses, ellipses2].

  Returns:  VOID
              No return type
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::GenerateRandomSetOfPointsOnGrid(size_t num1, size_t num2, D2D1::ColorF color1, D2D1::ColorF color2)
{
    RECT rc;
    GetClientRect(GetWindow(m_hwnd, GW_CHILD), &rc);
    int middleX = (rc.right - rc.left) / 2;
    int middleY = (rc.bottom - rc.top) / 2;
    for (size_t i = 0; i < num1; i++)
    {
        ellipses.emplace_back(GenerateRandomEllipse(color1, (int)(middleX * 1.35), (int)(middleY * .95), (int)((middleX * 1.05) > (350) ? (middleX * 1.05) : (350)), (int)(middleY * .55)));
    }
    for (size_t i = 0; i < num2; i++)
    {
        ellipses2.emplace_back(GenerateRandomEllipse(color2, (int)(middleX * 1.6), (int)(middleY * .55), (int)((middleX * 1.4) > (350) ? (middleX * 1.4) : (350)), (int)(middleY * .1)));
    }
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::GenerateOrigin

  Summary:  sets the initial position of the origin (used in MinkowskiSum, MinkowskiDiff, and GJK) to be at the center of the main window
    
  Args:     NONE

  Modifies: [graphOrigin].

  Returns:  VOID
              No return type, only sets the origin
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::GenerateOrigin()
{
    RECT rc;
    GetClientRect(GetWindow(m_hwnd, GW_CHILD), &rc);

    std::shared_ptr<MyEllipse> newEllipse = std::make_shared<MyEllipse>();
    newEllipse->ellipse = D2D1::Ellipse(D2D1::Point2F(rc.right / static_cast<FLOAT>(2), rc.bottom / static_cast<FLOAT>(2)), VERTEX_RADIUS, VERTEX_RADIUS);
    newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
    graphOrigin = newEllipse;
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:    MainWindow::ClearLists()

  Summary:  Clears the list of points and vertices

  Args:     NONE

  Modifies: [ellipses, ellipses2, ellipses3, convexHull, convexHull2, convexHull3].

  Returns:  VOID
              No return type
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::ClearLists()
{
    ellipses.clear();
    ellipses2.clear();
    ellipses3.clear();
    convexHull.clear();
    convexHull2.clear();
    convexHull3.clear();
}

void MainWindow::AlgoTest()
{
    switch (algoMode)
    {
    case AlgoMode::MinkowskiSum:
        QuickHull(ellipses, convexHull);
        QuickHull(ellipses2, convexHull2);
        MinkowskiSum(convexHull, convexHull2, ellipses3);
        QuickHull(ellipses3, convexHull3);
        break;
    case AlgoMode::MinkowskiDifference:
        QuickHull(ellipses, convexHull);
        QuickHull(ellipses2, convexHull2);
        MinkowskiDiff(convexHull, convexHull2, ellipses3);
        QuickHull(ellipses3, convexHull3);
        break;
    case AlgoMode::QuickHull:
        QuickHull(ellipses, convexHull);
        break;
    case AlgoMode::PointConvexHullIntersection:
        QuickHull(ellipses2, convexHull2);
        if (PointInConvexHull(ellipses.front()->GetVertexPointer(), convexHull2))
            ellipses.front()->color = D2D1::ColorF(D2D1::ColorF::Red);
        else
            ellipses.front()->color = D2D1::ColorF(D2D1::ColorF::Green);
        break;
    case AlgoMode::gjk:
        QuickHull(ellipses, convexHull);
        QuickHull(ellipses2, convexHull2);
        MinkowskiDiff(convexHull, convexHull2, ellipses3);
        QuickHull(ellipses3, convexHull3);
    }
}

/*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
       Function: IsRight

       Summary:  Takes cross product of points a,b,c to determine
                 if c is to the right of the line formed by a and b.

       Args:     shared_ptr<D2D_POINT_2F> a
                       left most point
                 shared_ptr<D2D_POINT_2F> b
                       right most point
                 shared_ptr<D2D_POINT_2F> c
                       point being checked

       Returns:  BOOL
                       true if c is to the right of the line formed by
                       a and b
    -----------------------------------------------------------------F-F*/
BOOL MainWindow::IsRight(shared_ptr<D2D_POINT_2F> a, shared_ptr<D2D_POINT_2F> b, shared_ptr<D2D_POINT_2F> c)
{
    return ((b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x)) > 0;
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:    MainWindow::QuickHull

  Summary:  Recursive QuickHull as demonstrated in pseudo code found in
            Computation Geometry in C by Joseph O'Rourke.  First search
            list of points and find the left and right most points. Then split
            the points into two sets of the points all above and all below the
            line formed by the left and right most points.  Call a recursive method
            which finds the point the is farthest vertically from the line between
            the left and right most points. Make two more recursive calls with the
            left and farthest point and the set of points to the left of their line,
            and with the farthest and right points and the set of points to the
            left of their line.

  Args:     const list<shared_ptr<MyEllipse>>& points
              List of points that the convex hull will form around
            list<shared_ptr<D2D_POINT_2F>>& convexHull
              List of vertices that make up the convex hull

  Modifies: [convexHull1,convexHull2].

  Returns:  void
                doesn't return a type, modifies one of the two lists of vertices representing a convex hull
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::QuickHull(const list<shared_ptr<MyEllipse>>& points, vector<shared_ptr<D2D_POINT_2F>>& convexHull)
{
    shared_ptr<D2D_POINT_2F>        left;
    shared_ptr<D2D_POINT_2F>        right;

    /*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        Function: LineDistance

        Summary:  Takes cross product of points a,b,c to determine
                  verticle distance of c is to the left of the line formed by
                  a and b.

        Args:     shared_ptr<D2D_POINT_2F> a
                        left most point
                  shared_ptr<D2D_POINT_2F> b
                        right most point
                  shared_ptr<D2D_POINT_2F> c
                        point being checked

        Returns:  int
                        absolute vaue of the verticle distanc of c
                        from the line formed by a and b
     -----------------------------------------------------------------F-F*/
    function<int(shared_ptr<D2D_POINT_2F>, shared_ptr<D2D_POINT_2F>, shared_ptr<D2D_POINT_2F>)> LineDistance = [](shared_ptr<D2D_POINT_2F> a, shared_ptr<D2D_POINT_2F> b, shared_ptr<D2D_POINT_2F> c)->int
    {
        return (int)abs((c->y - a->y) * (b->x - a->x) -
            (b->y - a->y) * (c->x - a->x));
    };
    /*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        Function: IsRightList

        Summary:  Scans a list of points to see if they are right of
                  the line formed by a and b

        Args:     shared_ptr<D2D_POINT_2F> a
                        left most point
                  shared_ptr<D2D_POINT_2F> b
                        right most point
                  const list<shared_ptr<MyEllipse>>& pointSet
                        points being checked

        Returns:  list<shared_ptr<MyEllipse>
                        set of  points to the right of line a and b
     -----------------------------------------------------------------F-F*/
    function<list<shared_ptr<MyEllipse>>(shared_ptr<D2D_POINT_2F>, shared_ptr<D2D_POINT_2F>, const list<shared_ptr<MyEllipse>>&)> IsRightList = [&](shared_ptr<D2D_POINT_2F> left, shared_ptr<D2D_POINT_2F> right, const list<shared_ptr<MyEllipse>>& pointSet)->list<shared_ptr<MyEllipse>>
    {
        list <shared_ptr<MyEllipse>> leftPointSet;
        for (auto const& point : pointSet)
        {
            if (IsRight(left, right, point->GetVertexPointer()))
            {
                leftPointSet.push_back(point);
            }
        }
        return leftPointSet;
    };
    /*F+F+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        Function: FindHull

        Summary:  Recursive method described in main function header.
                  Finds one side of the convex hull

        Args:     shared_ptr<D2D_POINT_2F> left
                        left most point
                  shared_ptr<D2D_POINT_2F> right
                        right most point
                  const list<shared_ptr<MyEllipse>>& pointSet
                        points being checked

        Returns:  void
                        base case reached when pointSet is empty
                        right point is added to convex hull
     -----------------------------------------------------------------F-F*/
    function<void(shared_ptr<D2D_POINT_2F>, shared_ptr<D2D_POINT_2F>, const list<shared_ptr<MyEllipse>>&)> FindHull = [&](shared_ptr<D2D_POINT_2F> left, shared_ptr<D2D_POINT_2F> right, const list<shared_ptr<MyEllipse>>& pointSet)->void
    {
        if (pointSet.empty())
        {
            convexHull.push_back(left);
            return;
        }

        shared_ptr<D2D_POINT_2F> top = pointSet.front()->GetVertexPointer();
        for (auto const& point : pointSet)
        {
            if (LineDistance(left, right, point->GetVertexPointer()) > LineDistance(left, right, top))
            {
                top = point->GetVertexPointer();
            }
        }
        FindHull(left, top, IsRightList(left, top, pointSet));
        FindHull(top, right, IsRightList(top, right, pointSet));

    };


    left = points.front()->GetVertexPointer();
    right = points.front()->GetVertexPointer();

    //testing purposes only
    shared_ptr<MyEllipse>leftPoint = points.front();
    shared_ptr<MyEllipse>rightPoint = points.front();
    shared_ptr<MyEllipse>topPoint = points.front();
    shared_ptr<MyEllipse>bottomPoint = points.front();


    for (auto const& point : points) {
        if (point->ellipse.point.x < left->x)
        {
            leftPoint = point;
            left = point->GetVertexPointer();
        }
        if (point->ellipse.point.x > right->x)
        {
            rightPoint = point;
            right = point->GetVertexPointer();
        }
        if (point->ellipse.point.y < bottomPoint->ellipse.point.y)
        {
            bottomPoint = point;
        }
        if (point->ellipse.point.y > topPoint->ellipse.point.y)
        {
            topPoint = point;
        }
    }

    //leftPoint->color = D2D1::ColorF(D2D1::ColorF::Red);
    //rightPoint->color = D2D1::ColorF(D2D1::ColorF::Blue);
    //topPoint->color = D2D1::ColorF(D2D1::ColorF::Red);
    //bottomPoint->color = D2D1::ColorF(D2D1::ColorF::Blue);

    FindHull(left, right, IsRightList(left, right, points));
    FindHull(right, left, IsRightList(right, left, points));
}

BOOL MainWindow::PointInConvexHull(shared_ptr<D2D_POINT_2F> point, const vector<shared_ptr<D2D_POINT_2F>>& convexHull)
{
    if (IsRight(convexHull[convexHull.size() - 1], convexHull[0], point))      
    {
    /*    OutputDebugStringW(L"Nope\n");*/
        return FALSE;
    } 

    int left = 0;
    int right = convexHull.size() - 1;
    int i = (right + left) / 2;

    while ((left <= right) && i != 0)
    {

        /*wchar_t s[256];
        _swprintf(s, L"current index %d, left %d, right %d, \n", i, left, right);
        OutputDebugStringW(s);*/
        if (IsRight(convexHull[i], convexHull[0], point) && !IsRight(convexHull[i + 1], convexHull[0], point))
            return IsRight(convexHull[i + 1], convexHull[i], point);
        if (IsRight(convexHull[i], convexHull[0], point))
            left = i;
        else
            right = i;
        
        i = (right + left) / 2;
        
    }
   /* OutputDebugStringW(L"Touch Grass\n");*/
    return FALSE;
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::MinkowskiSum

  Summary:  Finds the Minkowski Sum of two convex hulls, and puts all of the points generated into a list of MyEllipses.
            Based on the Minkowski Sum demo from the algorithms for games website.

  Args:     vector<shared_ptr<D2D_POINT_2F>>& convexHull
              List of vertices that make up the first convex hull
            vector<shared_ptr<D2D_POINT_2F>>& convexHull2
              List of vertices that make up the second convex hull
            list<shared_ptr<MyEllipse>>& points
              List of vertices that make up the resulting convex hull

  Modifies: [points].

  Returns:  void
                doesn't return a type, modifies a convex hull to represent the Minkowski sum of two other convex hulls
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::MinkowskiSum(const vector<shared_ptr<D2D_POINT_2F>>& convexHull,const vector<shared_ptr<D2D_POINT_2F>>& convexHull2, list<shared_ptr<MyEllipse>>& points)
{
    for (std::size_t i = 0; i < convexHull.size(); i++)
    {
        shared_ptr<D2D_POINT_2F> point1 = convexHull[i];
        for (std::size_t j = 0; j < convexHull2.size(); j++)
        {

            shared_ptr<D2D_POINT_2F> point2 = convexHull2[j];
            float x = point1->x + point2->x - graphOrigin->ellipse.point.x;
            float y = point1->y + point2->y - graphOrigin->ellipse.point.y;
            std::shared_ptr<MyEllipse> newEllipse = std::make_shared<MyEllipse>();
            newEllipse->ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), VERTEX_RADIUS, VERTEX_RADIUS);
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            points.push_back(newEllipse);
        }
    }
}

/*M+M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M+++M
  Method:   MainWindow::MinkowskiDiff

  Summary:  Finds the Minkowski Difference of two convex hulls, and puts all of the points generated into a list of MyEllipses.
            Based on the Minkowski Difference demo from the algorithms for games website.

  Args:     vector<shared_ptr<D2D_POINT_2F>>& convexHull
              List of vertices that make up the first convex hull
            vector<shared_ptr<D2D_POINT_2F>>& convexHull2
              List of vertices that make up the second convex hull
            list<shared_ptr<MyEllipse>>& points
              List of vertices that make up the resulting convex hull

  Modifies: [points].

  Returns:  void
                doesn't return a type, modifies a convex hull to represent the Minkowski difference of two other convex hulls
M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M---M-M*/
void MainWindow::MinkowskiDiff(const vector<shared_ptr<D2D_POINT_2F>>& convexHull,const vector<shared_ptr<D2D_POINT_2F>>& convexHull2, list<shared_ptr<MyEllipse>>& points)
{
    for (std::size_t i = 0; i < convexHull.size(); i++)
    {
        shared_ptr<D2D_POINT_2F> point1 = convexHull[i];
        for (std::size_t j = 0; j < convexHull2.size(); j++)
        {
            shared_ptr<D2D_POINT_2F> point2 = convexHull2[j];
            float x = point1->x - point2->x + graphOrigin->ellipse.point.x;
            float y = point1->y - point2->y + graphOrigin->ellipse.point.y;
            std::shared_ptr<MyEllipse> newEllipse = std::make_shared<MyEllipse>();
            newEllipse->ellipse = D2D1::Ellipse(D2D1::Point2F(x, y), VERTEX_RADIUS, VERTEX_RADIUS);
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            points.push_back(newEllipse);
        }
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
    if (algoMode != AlgoMode::PointConvexHullIntersection)
    {
        for (auto j = ellipses2.rbegin(); j != ellipses2.rend(); ++j)
        {
            if ((*j)->HitTest(x, y))
            {
                selection2 = (++j).base();
                selection1 = false;
                return TRUE;
            }
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

    if (!win.Create(L"ConvexHullAlgorithms", WS_OVERLAPPEDWINDOW))
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

    //HWND buttonContainer = CreateWindow(
    //    L"STATIC",
    //    L"OK",
    //    WS_BORDER | WS_CHILD | SS_WHITEFRAME | SS_GRAYRECT,
    //    0,
    //    0,
    //    BUTTONWIDTH + 40,
    //    BUTTONHEIGHT * 10,
    //    win.Window(),
    //    (HMENU)99,
    //    hInstance,
    //    NULL);
    RECT rc;
    GetClientRect(win.Window(), &rc);


    HWND renderArea = CreateWindow(
        L"STATIC",
        L"OK",
        WS_BORDER | WS_CHILD | SS_WHITEFRAME | SS_GRAYRECT | WS_VISIBLE,
        BUTTON_AREA_WIDTH,
        0,
        rc.right-BUTTON_AREA_WIDTH,
        rc.bottom,
        win.Window(),
        (HMENU)99,
        hInstance,
        NULL);
   

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
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING),         // y position 
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
        BUTTONYCOOR + (BUTTONHEIGHT + BUTTONPADDING) * 2,         // y position 
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

    case WM_MOUSEWHEEL:
        if (algoMode == AlgoMode::MinkowskiSum || algoMode == AlgoMode::MinkowskiDifference || algoMode == AlgoMode::gjk)
        {
            static int nDelta = 0;
            nDelta += GET_WHEEL_DELTA_WPARAM(wParam);
            if (abs(nDelta) >= WHEEL_DELTA)
            {
                if (nDelta > 0)
                {
                    if (zoomScale * 1.1 >= maxZoomScale) {
                        ScalePoints(maxZoomScale / zoomScale);
                        zoomScale = maxZoomScale;
                    }
                    else {
                        zoomScale *= 1.1f;
                        ScalePoints((float)1.1);
                    }
                }
                else
                {
                    if (zoomScale / 1.1 <= minZoomScale) {
                        ScalePoints(minZoomScale / zoomScale);
                        zoomScale = minZoomScale;
                    }
                    else {
                        zoomScale /= 1.1f;
                        ScalePoints((float)(1 / 1.1));
                    }
                }
                nDelta = 0;
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_SELECT_MODE:
            SetMode(SelectMode);
            break;

        case ID_TOGGLE_MODE:
            break;

        }
        //Handle Button Inputs to change the state of the application or close the application
    case BN_CLICKED:
        if (LOWORD(wParam) == MINKOWSKI_DIFFERENCE)
        {
            OutputDebugStringW(L"MINKOWSKI_DIFFERENCE\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
            GenerateRandomSetOfPointsOnGrid(6, 6, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Green));
            GenerateOrigin();
            algoMode = AlgoMode::MinkowskiDifference;
            AlgoTest();
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == MINKOWSKI_SUM)
        {
            OutputDebugStringW(L"MINKOWSKI_SUM\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
            GenerateRandomSetOfPointsOnGrid(6, 6, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Green));
            GenerateOrigin();
            algoMode = AlgoMode::MinkowskiSum;
            AlgoTest();
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == QUICK_HULL)
        {
            OutputDebugStringW(L"QUICK_HULL\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
            GenerateRandomSetOfPoints(10, 0, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Green));
            algoMode = AlgoMode::QuickHull;
            AlgoTest();
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == POINT_CONVEX_HULL_INTERSECTION)
        {
            OutputDebugStringW(L"POINT_CONVEX_HULL_INTERSECTION\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
            algoMode = AlgoMode::PointConvexHullIntersection;
            GenerateRandomSetOfPoints(1, 10, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Green));
            AlgoTest();
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == GJK)
        {
            OutputDebugStringW(L"GJK\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
            GenerateRandomSetOfPointsOnGrid(6, 6, D2D1::ColorF(D2D1::ColorF::Green), D2D1::ColorF(D2D1::ColorF::Green));
            GenerateOrigin();
            algoMode = AlgoMode::gjk;
            AlgoTest();
            OnPaint();
            break;
        }
        if (LOWORD(wParam) == EXIT)
        {
            OutputDebugStringW(L"EXIT\n");
            ResetZoom();
            ClearSelection();
            ClearLists();
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
