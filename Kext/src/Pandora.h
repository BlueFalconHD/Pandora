#ifndef KEXTRW_H
#define KEXTRW_H

#include <IOKit/IOService.h>

class Pandora : public IOService {
  OSDeclareFinalStructors(Pandora);

public:
  virtual bool start(IOService *provider) override;
  virtual void stop(IOService *provider) override;
};

#endif // KEXTRW_H
