#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

// Compare the shader's original 21-tap footprint with its paired 12-read form.
// This tests ideal bilinear arithmetic, not GPU filtering precision or throughput.
struct Image {
  std::array<double,35> pixels{};
  double sample(int x, int y) const {
    return pixels[size_t(std::clamp(y,0,6)*5 + std::clamp(x,0,4))];
    }
  double linear(double x, double y) const {
    const int ix = int(std::floor(x)), iy = int(std::floor(y));
    const double fx = x-ix, fy = y-iy;
    return std::lerp(std::lerp(sample(ix,iy),sample(ix+1,iy),fx),
                     std::lerp(sample(ix,iy+1),sample(ix+1,iy+1),fx),fy);
    }
  };

static double weight(double x) {
  if(x==0)
    return 1;
  constexpr double pi = std::numbers::pi;
  return 2*std::sin(pi*x)*std::sin(pi*x/2)/(pi*pi*x*x);
  }

static double original(const Image& image, int bx, int by, double fx, double fy) {
  double sum = 0;
  for(int x=-2; x<=2; ++x)
    for(int y=-2; y<=2; ++y)
      if(!(std::abs(x)==2 && std::abs(y)==2))
        sum += image.sample(bx+x,by+y)*weight(x-fx)*weight(y-fy);
  return sum;
  }

static double paired(const Image& image, int bx, int by, double fx, double fy) {
  double wx[5], wy[5];
  for(int i=-2; i<=2; ++i) {
    wx[i+2] = weight(i-fx);
    wy[i+2] = weight(i-fy);
    }
  const double cx = wx[2]+wx[3], cy = wy[2]+wy[3];
  const double ox[] = {-2,-1,std::clamp(wx[3]/cx,0.0,1.0),2};
  const double oy[] = {-2,-1,std::clamp(wy[3]/cy,0.0,1.0),2};
  const double mx[] = {wx[0],wx[1],cx,wx[4]};
  const double my[] = {wy[0],wy[1],cy,wy[4]};
  double sum = 0;
  int reads = 0;
  for(int x=0; x<4; ++x)
    for(int y=0; y<4; ++y) {
      if((x==0 || x==3) && (y==0 || y==3))
        continue;
      sum += image.linear(bx+ox[x],by+oy[y])*mx[x]*my[y];
      ++reads;
      }
  if(reads!=12)
    throw std::runtime_error("Expected twelve bilinear reads");
  return sum;
  }

int main() {
  double worst = 0;
  size_t cases = 0;
  for(int pattern=0; pattern<4; ++pattern) {
    Image image;
    for(size_t i=0; i<image.pixels.size(); ++i) {
      if(pattern==0) image.pixels[i] = 1;
      if(pattern==1) image.pixels[i] = i==17 ? 32 : 0;
      if(pattern==2) image.pixels[i] = i%2==0 ? 0 : 16;
      if(pattern==3) image.pixels[i] = double((i*97+31)%107)/3;
      }
    // Include clamped borders, odd image dimensions and subpixel phases.
    for(int by=-2; by<9; ++by)
      for(int bx=-2; bx<7; ++bx)
        for(int y=0; y<=16; ++y)
          for(int x=0; x<=16; ++x) {
            const double fx = x/16.0, fy = y/16.0;
            const double a = original(image,bx,by,fx,fy);
            const double b = paired(image,bx,by,fx,fy);
            const double error = std::abs(a-b);
            if(!std::isfinite(b) || error>1e-10) {
              std::cerr << "Lanczos mismatch: " << a << " versus " << b << '\n';
              return 1;
              }
            worst = std::max(worst,error);
            ++cases;
            }
    }
  std::cout << cases << " cases passed; maximum ideal-filter error " << worst << '\n';
  return 0;
  }
