#include "include/analysis/multiLevelAnalysis.h"
void MultLevelAnalyzer::addLevel(
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> coupledVarVec,
    MAPPING::Transform &T, ARCH::Level &L, bool doubleBufferFlag) {
  int spatialDimNum = L.getSpatialDimNum();
  for (auto var : coupledVarVec) {
    _allCoupledVarVec.push_back(var);
  }
  _coupledVarVecVec.push_back(coupledVarVec);
  extendT(coupledVarVec, spatialDimNum, T);
  extendCoupledVar(coupledVarVec, spatialDimNum);
  Analyzer analyzer =
      Analyzer(coupledVarVec, T, _I, _W, _O, L, doubleBufferFlag);
  if (!analyzer.constraintCheckAndBuildAnalyzer())
    _validFlags.push_back(false);
  else
    _validFlags.push_back(true);
  _analyzerSet.emplace_back(analyzer);
}
void MultLevelAnalyzer::addLevel(
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> coupledVarVec,
    ARCH::Level &L, bool doubleBufferFlag) {
  int spatialDimNum = L.getSpatialDimNum();
  for (auto var : coupledVarVec) {
    _allCoupledVarVec.push_back(var);
  }
  _coupledVarVecVec.push_back(coupledVarVec);
  MAPPING::Transform T(coupledVarVec.size());
  extendT(coupledVarVec, spatialDimNum, T);
  extendCoupledVar(coupledVarVec, spatialDimNum);
  Analyzer analyzer =
      Analyzer(coupledVarVec, T, _I, _W, _O, L, doubleBufferFlag);
  _validFlags.push_back(false);
  _analyzerSet.emplace_back(analyzer);
}

void MultLevelAnalyzer::extendT(
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> &coupledVarVec,
    int spatialDimNum, MAPPING::Transform &T) {
  if (spatialDimNum == 1) {
    T.addExtraSpatial();
  } else if (spatialDimNum == 0) {
    T.addExtraSpatial();
    T.addExtraSpatial();
  }
  if (coupledVarVec.size() == spatialDimNum) {
    T.addExtraTemporal();
  }
}
void MultLevelAnalyzer::extendCoupledVar(
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> &coupledVarVec,
    int spatialDimNum) {
  if (spatialDimNum == 1) {
    coupledVarVec.insert(coupledVarVec.begin(),
                         std::make_shared<WORKLOAD::Iterator>(
                             WORKLOAD::Iterator(0, 0, "tmpPEX")));
  } else if (spatialDimNum == 0) {
    coupledVarVec.insert(coupledVarVec.begin(),
                         std::make_shared<WORKLOAD::Iterator>(
                             WORKLOAD::Iterator(0, 0, "tmpPEY")));
    coupledVarVec.insert(coupledVarVec.begin(),
                         std::make_shared<WORKLOAD::Iterator>(
                             WORKLOAD::Iterator(0, 0, "tmpPEX")));
  }
  if (coupledVarVec.size() == 2) {
    coupledVarVec.push_back(
        std::make_shared<WORKLOAD::Iterator>(WORKLOAD::Iterator(0, 0, "tmpT")));
  }
}

bool MultLevelAnalyzer::compAndCheckRequiredDataSize(int level) {
  for (auto var : _allCoupledVarVec) {
    var->lock();
  }
  for (int i = 0; i <= level; i++) {
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> &oneLevelCoupledVarVec =
        _coupledVarVecVec[i];

    for (auto var : oneLevelCoupledVarVec) {
      var->unlock();
    }
  }
  bool ret = _analyzerSet[level].compAndCheckRequiredDataSize();
  for (auto var : _allCoupledVarVec) {
    var->unlock();
  }
  return ret;
}

bool MultLevelAnalyzer::checkRequiredDataSize() {
  int levelNum = _analyzerSet.size();
  for (int i = 0; i < levelNum; i++) {
    if (!compAndCheckRequiredDataSize(i))
      return false;
  }
  return true;
}

void MultLevelAnalyzer::getSubLevelEdge(
    int level, std::map<std::shared_ptr<WORKLOAD::Iterator>,
                        std::shared_ptr<WORKLOAD::Iterator>> &subLevelEdgeMap) {
  auto &curLevelCoupledVarVec = _analyzerSet[level].getCoupledVarVec();
  for (auto it = curLevelCoupledVarVec.rbegin();
       it != curLevelCoupledVarVec.rend(); it++) {
    if ((*it)->hasEdge()) {
      subLevelEdgeMap[(*it)->getCoupledIterator()] = *it;
    } else {
      if (subLevelEdgeMap.count((*it))) {
        subLevelEdgeMap.erase((*it));
      }
    }
  }
}
int MultLevelAnalyzer::compBaseIndex(int varNum,
                                     std::vector<std::vector<int>> &state,
                                     int i) {
  int tmp = 0;
  for (int j = 0; j < varNum; j++) {
    tmp *= 2;
    tmp += state[i][j];
  }
  return tmp;
}
int MultLevelAnalyzer::getLevelNum() { return _analyzerSet.size(); }

void MultLevelAnalyzer::changeEdgeByState(
    bool flag, int varNum, int i, std::vector<std::vector<int>> &state,
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> &varVec) {
  for (int j = 0; j < varNum; j++) {
    if (state[i][j]) {
      if (flag) {
        varVec[j]->setEdge();
      } else {
        varVec[j]->unsetEdge();
      }
    }
  }
}

void MultLevelAnalyzer::generateSublevelBaseResult(
    int level, std::vector<std::shared_ptr<AnalyzerResult>> &subLevelResultVec,
    std::map<std::shared_ptr<WORKLOAD::Iterator>,
             std::shared_ptr<WORKLOAD::Iterator>> &subLevelEdgeMap,
    std::vector<Base> &baseVec) {
  if (subLevelEdgeMap.empty()) {
    recusiveAnalysis(level - 1);
    auto subLevelResult = _analyzerSet[level - 1].getResult();
    subLevelResult->occTimes = _analyzerSet[level].getOccTimes();
    subLevelResultVec.push_back(subLevelResult);
    baseVec.push_back(Base(subLevelResult->delay,
                           _analyzerSet[level - 1].getTensorDimRange()));
  } else {
    std::vector<std::shared_ptr<WORKLOAD::Iterator>> curSubCoupledVarVec;
    for (auto &item : subLevelEdgeMap) {
      curSubCoupledVarVec.push_back(item.second);
    }
    _analyzerSet[level].setCurSubCoupledVarVec(curSubCoupledVarVec);
    baseVec = std::vector<Base>(1 << subLevelEdgeMap.size());

    std::vector<std::vector<int>> state;
    WORKLOAD::generateEdgeState(state, curSubCoupledVarVec);
    int stateNum = state.size();
    int varNum = curSubCoupledVarVec.size();
    for (int i = 0; i < stateNum; i++) {
      changeEdgeByState(1, varNum, i, state, curSubCoupledVarVec);
      recusiveAnalysis(level - 1);
      auto subLevelResult = _analyzerSet[level - 1].getResult();
      subLevelResult->occTimes = _analyzerSet[level].getOccTimes();
      subLevelResultVec.push_back(subLevelResult);
      baseVec[compBaseIndex(varNum, state, i)] = Base(
          subLevelResult->delay, _analyzerSet[level - 1].getTensorDimRange());
      changeEdgeByState(0, varNum, i, state, curSubCoupledVarVec);
    }
  }
}

void MultLevelAnalyzer::recusiveAnalysis(int level) {
  if (level != 0) {
    std::vector<std::shared_ptr<AnalyzerResult>> subLevelResultVec;
    std::map<std::shared_ptr<WORKLOAD::Iterator>,
             std::shared_ptr<WORKLOAD::Iterator>>
        subLevelEdgeMap;
    getSubLevelEdge(level, subLevelEdgeMap);
    std::vector<Base> baseVec;
    generateSublevelBaseResult(level, subLevelResultVec, subLevelEdgeMap,
                               baseVec);
    _analyzerSet[level].setBase(baseVec);
    _analyzerSet[level].oneAnalysis();
    _analyzerSet[level].setSubLevelResultVec(subLevelResultVec);
    compAndCheckRequiredDataSize(level);
  } else {

    std::vector<Base> baseVec;
    baseVec.push_back({_I.getDimNum(), _W.getDimNum(), _O.getDimNum()});
    _analyzerSet[0].setBase(baseVec);
    _analyzerSet[0].oneAnalysis();
    compAndCheckRequiredDataSize(0);
  }
}

void MultLevelAnalyzer::compMultiLevelReuslt(
    std::shared_ptr<AnalyzerResult> resultTreeRoot) {
  compMultiLevelReusltDFS(resultTreeRoot, _analyzerSet.size() - 1);
  int levelNum = _analyzerSet.size();
  for (int i = 0; i < levelNum; i++) {
    auto result = _resultSet[i];
    result->compRate = result->compCycle / result->compRate;
    result->PEUtilRate =
        float(result->activePEMultTimeNum) / result->totalPEMultTimeNum;
    result->totalBandWidth[ARCH::INPUT] =
        _analyzerSet[i].compTotalBandWidth(ARCH::INPUT);
    result->totalBandWidth[ARCH::WEIGHT] =
        _analyzerSet[i].compTotalBandWidth(ARCH::WEIGHT);
    result->totalBandWidth[ARCH::OUTPUT] =
        _analyzerSet[i].compTotalBandWidth(ARCH::OUTPUT);
  }
}

void MultLevelAnalyzer::compMultiLevelReusltDFS(
    std::shared_ptr<AnalyzerResult> node, int level) {
  int occTimes = node->occTimes;
  *_resultSet[level] += *node;
  for (auto item : node->subLevelResultVec) {
    item->occTimes *= occTimes;
    compMultiLevelReusltDFS(item, level - 1);
  }
}

void MultLevelAnalyzer::oneAnalysis() {
  if (!constraintCheck())
    return;
  int levelNum = getLevelNum();
  _resultSet.clear();
  for (int i = 0; i < levelNum; i++) {
    _resultSet.push_back(std::make_shared<AnalyzerResult>(AnalyzerResult()));
  }
  recusiveAnalysis(levelNum - 1);
  auto resultTreeRoot = _analyzerSet[levelNum - 1].getResult();
  resultTreeRoot->occTimes = 1;
  compMultiLevelReuslt(resultTreeRoot);

  outputCSV();
}

void MultLevelAnalyzer::outputCSV() {
  int levelNum = _analyzerSet.size();
  std::ofstream logFile;
  logFile.open("result.csv", std::ios::app);
  logFile << "level ,";
  logFile << "unique_output,";
  logFile << "reuse_output,";
  logFile << "total_output,";
  logFile << "reuseRate_output,";

  for (int j = 0; j < 2; j++) {
    logFile << std::string("unique_input_") + std::to_string(j) + ",";
    logFile << std::string("reuse_input_") + std::to_string(j) + ",";
    logFile << std::string("total_input_") + std::to_string(j) + ",";
    logFile << std::string("reuseRate_input_") + std::to_string(j) + ",";
  }
  logFile << "bufferSize_output,";
  for (int j = 0; j < 2; j++) {
    logFile << std::string("bufferSize_input_") + std::to_string(j) + ",";
  }
  logFile << "totalBandWidth_output,";
  for (int j = 0; j < 2; j++) {
    logFile << std::string("totalBandWidth_input_") + std::to_string(j) + ",";
  }
  logFile << "maxInitDelay_output,";
  for (int j = 0; j < 2; j++) {
    logFile << std::string("maxInitDelay_input_") + std::to_string(j) + ",";
  }
  logFile << "maxInitTimes,";
  logFile << "maxStableDelay_output,";
  for (int j = 0; j < 2; j++) {
    logFile << std::string("maxStableDelay_input_") + std::to_string(j) + ",";
  }
  logFile << std::string("maxStableCompDelay") + ",";
  logFile << std::string("delay") + ",";
  logFile << std::string("compCycleRate") + ",";
  logFile << "PEUtilRate" << std::endl;
  for (int i = 0; i < levelNum; i++) {
    logFile << std::to_string(i) + ',';
    logFile << std::to_string(_resultSet[i]->uniqueVolumn[2]) + ",";
    logFile << std::to_string(_resultSet[i]->reuseVolumn[2]) + ",";
    logFile << std::to_string(_resultSet[i]->totalVolumn[2]) + ",";
    logFile << std::to_string(float(_resultSet[i]->reuseVolumn[2]) /
                              _resultSet[i]->totalVolumn[2]) +
                   ",";
    for (int j = 0; j < 2; j++) {
      logFile << std::to_string(_resultSet[i]->uniqueVolumn[j]) + ",";
      logFile << std::to_string(_resultSet[i]->reuseVolumn[j]) + ",";
      logFile << std::to_string(_resultSet[i]->totalVolumn[j]) + ",";
      logFile << std::to_string(float(_resultSet[i]->reuseVolumn[j]) /
                                _resultSet[i]->totalVolumn[j]) +
                     ",";
    }
    logFile << std::to_string(_resultSet[i]->requiredDataSize[2]) + ",";
    for (int j = 0; j < 2; j++) {
      logFile << std::to_string(_resultSet[i]->requiredDataSize[j]) + ",";
    }
    logFile << std::to_string(_resultSet[i]->totalBandWidth[2]) + ",";
    for (int j = 0; j < 2; j++) {
      logFile << std::to_string(_resultSet[i]->totalBandWidth[j]) + ",";
    }
    logFile << std::to_string(_resultSet[i]->initDelay[2]) + ",";
    for (int j = 0; j < 2; j++) {
      logFile << std::to_string(_resultSet[i]->initDelay[j]) + ",";
    }
    logFile << std::to_string(_resultSet[i]->initTimes) + ",";
    logFile << std::to_string(_resultSet[i]->stableDelay[2]) + ",";
    for (int j = 0; j < 2; j++) {
      logFile << std::to_string(_resultSet[i]->stableDelay[j]) + ",";
    }
    logFile << std::to_string(_resultSet[i]->stableDelay[3]) + ",";
    logFile << std::to_string(_resultSet[i]->delay) + ",";
    logFile << std::to_string(_resultSet[i]->compRate) + ",";
    logFile << std::to_string(_resultSet[i]->PEUtilRate) << std::endl;
    // std::cout << _resultSet[i]->delay << std::endl;
  }
  logFile.close();
}

void MultLevelAnalyzer::outputLog(std::ofstream &logFile) {
  int levelNum = _analyzerSet.size();
  for (int i = 0; i < levelNum; i++) {
    if (i != 0)
      logFile << ",";
    logFile << "\"LEVEL" + std::to_string(i) + "\":\n{";
    _analyzerSet[i].outputConfig(logFile);
    _resultSet[i]->outputLog(logFile);
    logFile << "}";
  }
}