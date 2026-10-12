#include "physics/dynamicworld.h"
#include "game/serialize.h"
#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <cmath>
#include <cstdlib>
#include <iostream>

static void check(bool value, const char* message) {
  if(!value) {
    std::cerr << message << '\n';
    std::exit(1);
    }
  }

static zenkit::Mesh plane(zenkit::MaterialGroup group) {
  zenkit::Mesh mesh;
  zenkit::Material material;
  material.name = "TEST:SECTOR";
  material.group = group;
  material.disable_collision = false;
  mesh.materials.push_back(material);
  mesh.vertices = {{-1000,0,-1000}, {1000,0,-1000}, {0,0,1000}};
  mesh.polygons.vertex_indices = {0,1,2};
  if(group==zenkit::MaterialGroup::WATER)
    mesh.polygons.vertex_indices = {0,2,1};
  mesh.polygons.material_indices = {0};
  return mesh;
  }

int main() {
  static_assert(!std::is_move_constructible_v<Serialize>);
  std::vector<uint8_t> archive;
  {
    Tempest::MemWriter output(archive);
    Serialize writer(output);
    writer.setEntry("test");
    writer.write(uint32_t(123));
    }
  for(int i=0; i<1000; ++i) {
    Tempest::MemReader input(archive);
    Serialize reader(input);
    reader.setEntry("test");
    uint32_t value = 0;
    reader.read(value);
    check(value==123, "repeated archive readers preserve contents");
    }

  auto source = plane(zenkit::MaterialGroup::STONE);
  auto data = DynamicWorld::buildLandscape(source);
  std::weak_ptr<const DynamicWorld::Landscape> lifetime = data;
  source = zenkit::Mesh();
  auto first = std::make_unique<DynamicWorld>(nullptr,data);
  auto second = std::make_unique<DynamicWorld>(nullptr,data);
  data.reset();
  check(!lifetime.expired(), "live worlds keep shared terrain alive after cache eviction");
  first.reset();
  const auto hit = second->landRay({0,100,0},200);
  check(hit.hasCol && std::abs(hit.v.y)<0.01f, "collision survives destroying the previous world");
  check(hit.mat==zenkit::MaterialGroup::STONE, "shared terrain preserves material");
  check(hit.sector!=nullptr && std::string_view(hit.sector)=="TEST:SECTOR", "sector names remain owned by terrain");
  second.reset();
  check(lifetime.expired(), "evicted terrain is released after its final world");

  auto water = DynamicWorld::buildLandscape(plane(zenkit::MaterialGroup::WATER));
  for(int i=0; i<20; ++i) {
    DynamicWorld world(nullptr,water);
    check(world.waterRay({0,-50,0},0).hasCol, "water-only worlds can be recreated without a land body");
    }
  auto empty = DynamicWorld::buildLandscape(zenkit::Mesh());
  DynamicWorld world(nullptr,empty);
  check(!world.landRay({0,100,0},200).hasCol, "empty worlds remain valid");
  std::cout << "World cache collision tests passed\n";
  }
