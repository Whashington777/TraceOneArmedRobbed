#pragma once
namespace trace::runtime {
template<class T> struct OverrideValue {
 T baseline,last;
 T Apply(T current,T desired,bool follow_game,bool multiplier){
  if(follow_game&&current!=last)baseline=current;
  last=multiplier?T(baseline*desired):desired;
  return last;
 }
 T Restore(T current)const{return current==last?baseline:current;}
};
}
