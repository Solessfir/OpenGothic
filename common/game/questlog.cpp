#include "questlog.h"
#include "serialize.h"

#include <algorithm>

QuestLog::QuestLog() {
  }

QuestLog::Quest &QuestLog::add(std::string_view name, Section s) {
  if(auto m = find(name))
    return *m;
  Quest q;
  q.name    = name;
  q.section = s;
  quests.emplace_back(q);
  return quests.back();
  }

void QuestLog::setStatus(std::string_view name, QuestLog::Status s) {
  auto m = find(name);
  if(m==nullptr && s==Status::Obsolete)
    return;
  auto& q  = add(name,Mission);
  q.status = s;
  }

void QuestLog::addEntry(std::string_view name, std::string_view entry) {
  if(auto m = find(name))
    m->entry.emplace_back(entry);
  }

void QuestLog::moveToTop(std::string_view name) {
  for(size_t i=0; i<quests.size(); ++i)
    if(quests[i].name==name) {
      std::rotate(quests.begin()+ptrdiff_t(i),quests.begin()+ptrdiff_t(i)+1,quests.end());
      return;
      }
  }

QuestLog::Quest *QuestLog::find(std::string_view name) {
  for(auto& i:quests)
    if(i.name==name)
      return &i;
  return nullptr;
  }

void QuestLog::save(Serialize &fout) {
  uint32_t sz=uint32_t(quests.size());
  fout.write(sz);
  for(auto& i:quests){
    fout.write(i.name,uint8_t(i.section),uint8_t(i.status),i.entry);
    }
  }

void QuestLog::load(Serialize &fin) {
  uint32_t sz=0;
  fin.read(sz);
  quests.resize(sz);

  for(auto& i:quests){
    fin.read(i.name,reinterpret_cast<uint8_t&>(i.section),reinterpret_cast<uint8_t&>(i.status),i.entry);
    }
  }
