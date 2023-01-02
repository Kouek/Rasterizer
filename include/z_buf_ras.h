#ifndef KOUEK_Z_BUF_RAS_H
#define KOUEK_Z_BUF_RAS_H

#include <rasterizer.h>

namespace kouek {

class ZBufferRasterizer : virtual public Rasterizer {
  public:
    virtual ~ZBufferRasterizer() {}

    static std::unique_ptr<Rasterizer> Create();
};

} // namespace kouek

#endif // !KOUEK_Z_BUF_RAS_H
