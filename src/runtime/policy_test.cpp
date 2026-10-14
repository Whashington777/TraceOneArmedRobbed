#include "override_value.hpp"
#include <iostream>
#include <stdexcept>
using trace::runtime::OverrideValue;
static void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
 try{
 OverrideValue<float> speed{350,350};Check(speed.Apply(350,2,true,true)==700,"walk speed");Check(speed.Apply(700,2,true,true)==700,"multiplier must not compound");Check(speed.Apply(525,2,true,true)==1050,"follow sprint state");Check(speed.Restore(1050)==525,"restore sprint baseline");Check(speed.Restore(0)==0,"preserve newer game state");
 OverrideValue<int> ammo{7,7};Check(ammo.Apply(7,30,true,false)==30,"refill");Check(ammo.Apply(29,30,true,false)==30,"refill after shot");Check(ammo.Restore(30)==29,"restore most recent real ammo state");
 OverrideValue<float> cooldown{30,30};Check(cooldown.Apply(30,0,false,false)==0,"disable cooldown");Check(cooldown.Restore(0)==30,"restore cooldown");
 OverrideValue<float> lock{.3f,.3f};auto result=lock.Apply(.3f,10,false,true);Check(result>2.99f&&result<3.01f,"lock multiplier");Check(lock.Apply(result,10,false,true)==result,"lock must not compound");
 std::cout<<"Movement transitions, non-compounding multipliers, ammo and restore policy passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
