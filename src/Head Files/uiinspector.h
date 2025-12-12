#pragma once

#include <UIAutomation.h>
#include <windows.h>
#include <QPoint>
#include <QRect>
#include <QString>

class UIInspector
{
public:
    UIInspector();
    ~UIInspector();

    QRect quickInspect(const POINT& pt, HWND excludeHwnd = nullptr);
    QRect quickInspect(const QPoint& pt, HWND excludeHwnd = nullptr);

private:
    QRect getElementRect(IUIAutomationElement* element);
    IUIAutomationElement* findBestElementAtPoint(
        IUIAutomationElement* root,
        const POINT& pt);
    IUIAutomation* m_automation;
};