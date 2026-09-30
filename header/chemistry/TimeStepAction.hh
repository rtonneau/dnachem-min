#ifndef ITACTION_H
#define ITACTION_H

#include "G4UserTimeStepAction.hh"
#include "scoring/ReactionCounter.hh"

class TimeStepAction : public G4UserTimeStepAction
{
public:
  TimeStepAction();
  virtual ~TimeStepAction();
  TimeStepAction(const TimeStepAction &other);
  TimeStepAction &operator=(const TimeStepAction &other);

  virtual void StartProcessing();

  virtual void UserPreTimeStepAction();
  virtual void UserPostTimeStepAction();

  virtual void UserReactionAction(const G4Track &, const G4Track &, const std::vector<G4Track *> *);

  virtual void EndProcessing();

  ReactionCounter &GetReactionCounter() { return fReactionCounter; }

private:
  ReactionCounter fReactionCounter;
};

#endif // ITACTION_H
