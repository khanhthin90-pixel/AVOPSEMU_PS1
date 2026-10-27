#pragma once
#include <windows.h>
namespace ps1ui {
constexpr COLORREF background=RGB(15,16,24),panel=RGB(24,25,38),border=RGB(48,49,68),text=RGB(239,239,249),muted=RGB(162,164,187),accent=RGB(169,157,255);
inline void box(HDC dc,RECT r,COLORREF fill,COLORREF line,int radius=14){
 auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,1,line);auto oldb=SelectObject(dc,brush),oldp=SelectObject(dc,pen);
 RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,oldb);SelectObject(dc,oldp);DeleteObject(brush);DeleteObject(pen);
}
inline void label(HDC dc,const wchar_t* value,RECT r,HFONT font,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE){
 auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,value,-1,&r,flags);SelectObject(dc,old);
}
}
