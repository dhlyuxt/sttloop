#pragma once

#include "include/datastruct/arch.h"
#include "include/datastruct/mapping.h"
#include "include/datastruct/result.h"
#include "include/datastruct/workload.h"
#include "include/util/debug.h"
#include "include/util/eigenUtil.h"
#include "include/util/timeline.h"
#include <set>
#include <vector>

class Analyzer {
private:
  std::vector<std::shared_ptr<WORKLOAD::Iterator>> &_coupledVarVec;
  MAPPING::Transform &_T;
  WORKLOAD::Tensor &_I;
  WORKLOAD::Tensor &_W;
  WORKLOAD::Tensor &_O;
  ARCH::Level &_L;
  MAPPING::Access _accessI;
  MAPPING::Access _accessW;
  MAPPING::Access _accessO;
  std::shared_ptr<Result> _result;
  std::shared_ptr<std::vector<std::vector<int>>> _reuseVecI;
  std::shared_ptr<std::vector<std::vector<int>>> _reuseVecW;
  std::shared_ptr<std::vector<std::vector<int>>> _reuseVecO;
  std::map<ARCH::DATATYPE, std::shared_ptr<std::vector<std::vector<int>>>>
      _reuseVecMap;
  std::shared_ptr<WORKLOAD::Iterator> PEX;
  std::shared_ptr<WORKLOAD::Iterator> PEY;
  bool _doubleBufferFlag;

  std::vector<Base> _baseSet;
  std::vector<std::shared_ptr<WORKLOAD::Iterator>> _curSubCoupledVarVec;
  std::set<std::shared_ptr<WORKLOAD::Iterator>> _curSubCoupledVarSet;
  int _curBaseIndex;

  std::pair<int, int> compTRange(int row);
  bool checkValidInnerDim(int varIndex, ARCH::DATATYPE dataType);
  void constructSingleDataTypeTimeSet(int flag,
                                      std::set<int> &singleDataTypeSet,
                                      ARCH::DATATYPE dataType);
  void constructInnerOuterTimeVec(std::vector<int> &innerTimeVec,
                                  std::vector<int> &outerTimeVec);
  int compOneStateTimeSize(std::vector<int> &timeVec);
  void generateVarVec(std::vector<int> &timeVec,
                      std::vector<std::shared_ptr<WORKLOAD::Iterator>> &varVec);
  void compOneDataVolumn(ARCH::DATATYPE dataType, MAPPING::Access &access,
                         std::vector<int> &innerTimeVec, int outerTimeSize,
                         WORKLOAD::Tensor &curTensor);
  int compOneStateStableDelay();
  int compOneStateInitDelay(std::pair<int, int> &PEXRange,
                            std::pair<int, int> &PEYRange);
  void compOneStateDelay(std::vector<int> &innerTimeVec, int &delay,
                         int &compCycle, int &initDelay,
                         int &activePEMultTimeNum);
  void accessAnalysis(std::vector<int> &innerTimeVec,
                      std::vector<int> &outerTimeVec);
  void delayAnalysis(std::vector<int> &innerTimeVec,
                     std::vector<int> &outerTimeVec);
  void setEdge(std::shared_ptr<WORKLOAD::Iterator> curI);
  void unsetEdge(std::shared_ptr<WORKLOAD::Iterator> curI);
  void changeBase();
  void compOneStateVolumn(int &uniqueVolumn, int &totalVolumn,
                          std::vector<int> &innerTimeVec,
                          ARCH::DATATYPE dataType, WORKLOAD::Tensor &curTensor);
  void
  changeEdgeByState(bool flag, int varNum, int i,
                    std::vector<std::vector<int>> &state,
                    std::vector<std::shared_ptr<WORKLOAD::Iterator>> &varVec);

public:
  Analyzer(std::vector<std::shared_ptr<WORKLOAD::Iterator>> &coupledVarVec,
           MAPPING::Transform &T, WORKLOAD::Tensor &I, WORKLOAD::Tensor &W,
           WORKLOAD::Tensor &O, ARCH::Level &L, bool doubleBufferFlag)
      : _coupledVarVec(coupledVarVec), _T(T), _I(I), _W(W), _O(O), _L(L),
        _accessI(MAPPING::constructAccessMatrix(I, coupledVarVec)),
        _accessW(MAPPING::constructAccessMatrix(W, coupledVarVec)),
        _accessO(MAPPING::constructAccessMatrix(O, coupledVarVec)),
        _doubleBufferFlag(doubleBufferFlag) {
    DEBUG::check(T.check(), DEBUG::TMATRIXERROR, T.to_string());
    if (!_accessI.isScalar())
      _reuseVecI = compReuseVec(T, _accessI);
    else
      _reuseVecI = scalarReuseVec(_coupledVarVec.size());
    if (!_accessW.isScalar())
      _reuseVecW = compReuseVec(T, _accessW);
    else
      _reuseVecW = scalarReuseVec(_coupledVarVec.size());
    if (!_accessO.isScalar())
      _reuseVecO = compReuseVec(T, _accessO);
    else
      _reuseVecO = scalarReuseVec(_coupledVarVec.size());
    // getTimeLine(coupledVarVec, T, I, W, O);

    int colNum = _T.getColNum();
    for (int i = 0; i < colNum; i++) {
      if (_T(0, i) == 1)
        PEX = _coupledVarVec[i];
      if (_T(1, i) == 1)
        PEY = _coupledVarVec[i];
    }
    std::pair<int, int> PEXRange = compTRange(0);
    std::pair<int, int> PEYRange = compTRange(1);
    DEBUG::check(_L.checkPEDimRange(PEXRange, 1), DEBUG::PEDIMOUTOFRANGE,
                 PEX->to_string());
    DEBUG::check(_L.checkPEDimRange(PEYRange, 0), DEBUG::PEDIMOUTOFRANGE,
                 PEY->to_string());

    DEBUG::check(!PEX->hasEdge(), DEBUG::EDGEPEDIMERROR, PEX->to_string());
    DEBUG::check(!PEY->hasEdge(), DEBUG::EDGEPEDIMERROR, PEY->to_string());
    _reuseVecMap[ARCH::INPUT] = _reuseVecI;
    _reuseVecMap[ARCH::WEIGHT] = _reuseVecW;
    _reuseVecMap[ARCH::OUTPUT] = _reuseVecO;
    DEBUG::check(_L.checkNetworkReuseValid(ARCH::INPUT, _reuseVecI),
                 DEBUG::REUSEVECNOTFITNETWORKARCHERROR, "Input");
    DEBUG::check(_L.checkNetworkReuseValid(ARCH::WEIGHT, _reuseVecW),
                 DEBUG::REUSEVECNOTFITNETWORKARCHERROR, "Weight");
    DEBUG::check(_L.checkNetworkReuseValid(ARCH::OUTPUT, _reuseVecO),
                 DEBUG::REUSEVECNOTFITNETWORKARCHERROR, "Output");
  }
  void setCurSubCoupledVarVec(std::vector<std::shared_ptr<WORKLOAD::Iterator>>
                                  curSubCoupledVarVec = {});
  void setBase(std::vector<Base> baseSet);
  std::vector<std::shared_ptr<WORKLOAD::Iterator>> &getCoupledVarVec();
  void oneAnalysis();
  void compRequiredDataSize();
  int compTotalBandWidth(ARCH::DATATYPE dataType);
  std::shared_ptr<Result> getResult();
  int getOccTimes();
  void
  setSubLevelResultVec(std::vector<std::shared_ptr<Result>> &subLevelResultVec);
};