#include "UIInspector.h"
#include <QDebug>
#include <vector>
#include <limits>
UIInspector::UIInspector()
    : m_automation(nullptr)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        qWarning() << "[UIInspector] CoInitializeEx FAILED!";
    }

    hr = CoCreateInstance(__uuidof(CUIAutomation),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        reinterpret_cast<void**>(&m_automation));

    if (FAILED(hr) || !m_automation) {
        qWarning() << "[UIInspector] CoCreateInstance FAILED, UIA unusable!";
        m_automation = nullptr;
    }
}

UIInspector::~UIInspector()
{
    if (m_automation) {
        m_automation->Release();
        m_automation = nullptr;
    }
    CoUninitialize();
}
QRect UIInspector::quickInspect(const POINT& pt, HWND /*excludeHwnd*/)
{
    if (!m_automation)
        return QRect();

    HRESULT hr = S_OK;
    IUIAutomationElement* element = nullptr;

    // ----------直接用 UIAutomation 从坐标取 element ----------
    hr = m_automation->ElementFromPoint(pt, &element);

    if (SUCCEEDED(hr) && element) {
        int elementPid = 0;
        element->get_CurrentProcessId(&elementPid);

        RECT uiaRect{};
        HRESULT hrRect = element->get_CurrentBoundingRectangle(&uiaRect);

        // 命中的是“别的进程里”的元素，并且矩形有效：直接用它
        if (elementPid != GetCurrentProcessId()
            && SUCCEEDED(hrRect)
            && !IsRectEmpty(&uiaRect)) {

            QRect r(uiaRect.left,
                uiaRect.top,
                uiaRect.right - uiaRect.left,
                uiaRect.bottom - uiaRect.top);
            element->Release();
            return r;
        }

        // 命中自己进程，或者矩形为 0：后面走 fallback
        element->Release();
        element = nullptr;
    }

    // ---------- WindowFromPoint + Z 序，跳过本进程和无效窗口 ----------

    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) {
        return QRect();
    }

    DWORD selfPid = GetCurrentProcessId();
    int   guard = 0;   // 防止死循环

    while (hwnd && guard < 200) {
        ++guard;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);

        wchar_t title[256] = { 0 };
        GetWindowTextW(hwnd, title, 255);

        // 1. 跳过本进程的窗口（截图 overlay、自家控件）
        if (pid == selfPid) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }

        // 2. 跳过不可见窗口
        if (!IsWindowVisible(hwnd)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }

        // 3. 跳过 0 大小窗口
        RECT winRect{};
        if (!GetWindowRect(hwnd, &winRect) || IsRectEmpty(&winRect)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }

        // 4. 如果这个窗口矩形不包含当前点，也跳过
        if (!PtInRect(&winRect, pt)) {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
            continue;
        }

        // 找到了一个“看起来靠谱”的 hwnd，跳出循环
        break;
    }

    if (!hwnd) {
        return QRect();
    }

    // ---------- 把 hwnd 转成 UIA element，并在子树中找“最贴近鼠标点”的子元素 ----------

    hr = m_automation->ElementFromHandle(hwnd, &element);
    if (FAILED(hr) || !element) {
        return QRect();
    }

    // 1) 在 root 子树里查找包含 pt 且面积最小的 child element
    IUIAutomationElement* best = findBestElementAtPoint(element, pt);
    QRect resultRect;

    if (best) {
        RECT bestRect{};
        HRESULT hrBest = best->get_CurrentBoundingRectangle(&bestRect);
        if (SUCCEEDED(hrBest) && !IsRectEmpty(&bestRect)) {
            resultRect = QRect(bestRect.left,
                bestRect.top,
                bestRect.right - bestRect.left,
                bestRect.bottom - bestRect.top);
        }
        best->Release();
    }

    // 2) 如果没找到合适 child，就退回到整个窗口的矩形
    if (!resultRect.isValid() || resultRect.isNull()) {
        RECT rootRect{};
        HRESULT hrRoot = element->get_CurrentBoundingRectangle(&rootRect);
        if (SUCCEEDED(hrRoot) && !IsRectEmpty(&rootRect)) {
            resultRect = QRect(rootRect.left,
                rootRect.top,
                rootRect.right - rootRect.left,
                rootRect.bottom - rootRect.top);
        }
    }

    element->Release();
    return resultRect;
}
IUIAutomationElement* UIInspector::findBestElementAtPoint(
    IUIAutomationElement* root,
    const POINT& pt)
{
    if (!root || !m_automation)
        return nullptr;

    IUIAutomationTreeWalker* walker = nullptr;
    HRESULT hr = m_automation->get_ControlViewWalker(&walker);
    if (FAILED(hr) || !walker)
        return nullptr;

    // DFS 栈
    std::vector<IUIAutomationElement*> stack;
    root->AddRef();                // root +1，后面统一 Release
    stack.push_back(root);

    IUIAutomationElement* best = nullptr;
    LONG bestArea = std::numeric_limits<LONG>::max();

    // 限制遍历的节点数量，避免在复杂 UI 上卡死
    const int  maxNodes = 00;      // 你可以根据体验再调，比如 400~800
    int        nodeVisited = 0;

    // “够小就别再往下钻太深”，比如 30x30 的控件已经很细了
    const LONG goodEnoughArea = 30 * 30;

    while (!stack.empty()) {
        IUIAutomationElement* cur = stack.back();
        stack.pop_back();
        ++nodeVisited;

        RECT rc{};
        HRESULT hrRect = cur->get_CurrentBoundingRectangle(&rc);
        if (SUCCEEDED(hrRect) && !IsRectEmpty(&rc)) {
            if (PtInRect(&rc, pt)) {
                LONG w = rc.right - rc.left;
                LONG h = rc.bottom - rc.top;
                LONG area = w * h;

                if (area > 0 && area < bestArea) {
                    if (best) best->Release();
                    best = cur;
                    best->AddRef();
                    bestArea = area;
                }
            }
        }

        // 是否还需要继续向下遍历子节点？
        bool needExploreChildren =
            (nodeVisited < maxNodes) &&         // 已到节点上限就不再深入
            (bestArea > goodEnoughArea);        // 已经很小了就不再深入

        if (needExploreChildren) {
            IUIAutomationElement* child = nullptr;
            hr = walker->GetFirstChildElement(cur, &child);
            while (SUCCEEDED(hr) && child) {
                // 入栈前先 AddRef，之后在弹出时 Release
                child->AddRef();
                stack.push_back(child);

                IUIAutomationElement* sibling = nullptr;
                hr = walker->GetNextSiblingElement(child, &sibling);
                child->Release();
                child = sibling;
            }
        }

        cur->Release();  // 和 push 时的 AddRef 对应

        if (nodeVisited >= maxNodes) {
            // 不再继续，从栈中把剩下的元素 Release 掉，避免内存泄漏
            for (IUIAutomationElement* e : stack) {
                if (e) e->Release();
            }
            stack.clear();
            break;
        }
    }

    walker->Release();
    return best; // 调用者负责 Release
}

QRect UIInspector::quickInspect(const QPoint& pt, HWND excludeHwnd)
{
    POINT p{ pt.x(), pt.y() };
    return quickInspect(p, excludeHwnd);
}

QRect UIInspector::getElementRect(IUIAutomationElement* element)
{
    RECT rect;
    element->get_CurrentBoundingRectangle(&rect);
    return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}