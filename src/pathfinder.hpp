#pragma once

#include "includes.hpp"

class PathFinder {
public:
  static auto &get() {
    static PathFinder instance;
    return instance;
  }

  static void start();
  static void stop(std::string const &message = "", bool error = false);
  static void onFrame(int frame);
  static void onDeath(int frame);
  static void onLevelComplete(int frame);
  static void loadSettings();
  static bool isRunning();

private:
  bool running = false;
  bool handlingDeath = false;
  bool previousStopPlaying = false;
  int bestFrame = 0;
  int currentAttemptBest = 0;
  int backtrackStep = 4;
  int maxShiftPerAction = 6;
  int maxActionsBack = 8;
  int warmupFrames = 12;
  int minActionFrame = 6;
  int minProgressForRecord = 2;
  int minSolveFrame = 30;
  int searchWindowFrames = 120;
  int addAttempts = 0;
  int maxAddedActions = 24;
  int restartSerial = 0;
  int lastHandledDeathSerial = -1;
  bool attemptArmed = false;
  bool spawnCaptured = false;
  cocos2d::CCPoint spawnPoint = cocos2d::CCPointZero;
  std::unordered_map<std::string, int> shiftAttempts;

#ifdef GEODE_IS_WINDOWS
  bool consoleEnabled = true;
  bool consoleOpen = false;
#endif
};
