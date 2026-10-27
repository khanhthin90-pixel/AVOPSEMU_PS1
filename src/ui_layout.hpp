#pragma once
#include <algorithm>
namespace avopsui {
struct Rect { int x=0,y=0,width=0,height=0; };
struct Layout { Rect sidebar,screen,debug,hint,status; };
inline Layout layout(int width,int height,bool cinema,bool debugger){
 width=std::max(800,width);height=std::max(600,height);
 const int left=cinema?20:264, right=width-20, top=156, bottom=height-116;
 const int available=right-left;
 const int debug_width=debugger?std::min(380,available/2):0;
 Layout l;
 l.sidebar={20,top,224,bottom-top};
 l.screen={left,top,available-(debugger?debug_width+16:0),bottom-top};
 l.debug={right-debug_width,top,debug_width,bottom-top};
 l.hint={left,height-100,available,42};
 l.status={20,height-44,width-40,30};
 return l;
}
}
