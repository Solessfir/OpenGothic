#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

class MultiFingerTap {
  public:
    void down(int pointer, float x, float y, uint64_t now, bool eligible) {
      if(contacts.empty()) {
        started=now;
        peak=0;
        valid=true;
        releasing=false;
        }
      if(contacts.contains(pointer)) { valid=false; return; }
      contacts.emplace(pointer,Contact{x,y});
      peak=std::max(peak,int(contacts.size()));
      valid &= eligible && !releasing && now-started<=180 && peak<=4;
      }

    void move(int pointer, float x, float y, float slop) {
      const auto it=contacts.find(pointer);
      if(it!=contacts.end() && std::hypot(x-it->second.x,y-it->second.y)>slop)
        valid=false;
      }

    int up(int pointer, uint64_t now) {
      if(contacts.erase(pointer)==0) return 0;
      releasing=true;
      if(!contacts.empty() || !valid || now-started>400) return 0;
      return peak==3 || peak==4 ? peak : 0;
      }

    bool ready() const { return valid && !releasing && contacts.size()>=3; }
    bool empty() const { return contacts.empty(); }
    void reset() { contacts.clear(); valid=false; }

  private:
    struct Contact { float x,y; };
    std::unordered_map<int,Contact> contacts;
    uint64_t started=0;
    int peak=0;
    bool valid=false;
    bool releasing=false;
  };
