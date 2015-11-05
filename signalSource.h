
class SignalSource
{

 public:
  virtual ~SignalSource() {}
  virtual bool Start() {}
  virtual bool GetNextSamples(int16_t sample_buffer[][2], double_t & centerFrequency) = 0;
  virtual bool Stop() {}
};
