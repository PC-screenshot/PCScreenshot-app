#include "UIInspector.h"
#include <QDebug>

UIInspector::UIInspector()
    : m_automation(nullptr)
{
    // 使用 CoInitializeEx 并指定单线程公寓（STA），UI Automation 在 UI 线程上应该是 STA。
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        qWarning() << "CoInitializeEx failed:" << QString::number(hr, 16);
        // 仍尝试继续（某些情况下会返回 S_FALSE 表示已初始化）
    }

    // 创建 UIAutomation 实例
    hr = CoCreateInstance(__uuidof(CUIAutomation),
        nullptr,
        CLSCTX_INPROC_SERVER,
        __uuidof(IUIAutomation),
        reinterpret_cast<void**>(&m_automation));
    if (FAILED(hr)) {
        qWarning() << "CoCreateInstance(CUIAutomation) failed:" << QString::number(hr, 16);
        // 如果这里失败，后续 ElementFromPoint 会失败
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

QRect UIInspector::quickInspect(const POINT& pt)
{
    if (!m_automation)
        return QRect();

    IUIAutomationElement* element = nullptr;
    HRESULT               hr = m_automation->ElementFromPoint(pt, &element);

    if (SUCCEEDED(hr) && element) {
        auto qrect = getElementRect(element);
        //        qDebug() << "result: " << getControlTypeName(controlType) << elementName << qrect;
        element->Release();
        return qrect;
    }

    return QRect();
}

QRect UIInspector::quickInspect(const QPoint& pt)
{
    if (!m_automation)
        return QRect();

    POINT point;
    point.x = pt.x();
    point.y = pt.y();
    IUIAutomationElement* element = nullptr;
    HRESULT               hr = m_automation->ElementFromPoint(point, &element);

    if (SUCCEEDED(hr) && element) {
        auto qrect = getElementRect(element);
        element->Release();
        return qrect;
    }

    return QRect();
}

QRect UIInspector::getElementRect(IUIAutomationElement* element)
{
    RECT rect;
    element->get_CurrentBoundingRectangle(&rect);
    return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}