#include "ui_layout.hpp"
#include <cassert>
#include <iostream>
int main(){
 for(int w:{1000,1280,1440,1920,2560})for(int h:{720,900,1440})
 for(bool cinema:{false,true})for(bool debug:{false,true}){
  const auto l=avopsui::layout(w,h,cinema,debug);
  assert(l.screen.width>0&&l.screen.height>0);
  assert(l.screen.x+l.screen.width<=w-20);
  assert(l.screen.y+l.screen.height<l.hint.y);
  assert(l.hint.y+l.hint.height<l.status.y);
  if(!cinema)assert(l.sidebar.x+l.sidebar.width<l.screen.x);
  if(debug)assert(l.screen.x+l.screen.width<l.debug.x&&l.debug.x+l.debug.width<=w-20);
  else assert(l.debug.width==0);
 }
 std::cout<<"AVOPSEMU layout bounds passed\n";
}
