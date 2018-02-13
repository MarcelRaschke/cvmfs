
#include <gmock/gmock.h>

#include "sync_mediator.h"

#include "hash.h"

namespace publish {

class MockSyncMediator : public AbstractSyncMediator {
 public:
  MockSyncMediator() : AbstractSyncMediator() {}
  MockSyncMediator(catalog::WritableCatalogManager *catalog_manager,
                   const SyncParameters *params)
      : AbstractSyncMediator() {}

  MOCK_METHOD1(RegisterUnionEngine, void(SyncUnion *engine));
  MOCK_METHOD1(Add, void(const SyncItem &entry));
  MOCK_METHOD1(Touch, void(const SyncItem &entry));
  MOCK_METHOD1(Remove, void(const SyncItem &entry));
  MOCK_METHOD1(Replace, void(const SyncItem &entry));
  MOCK_METHOD1(EnterDirectory, void(const SyncItem &entry));
  MOCK_METHOD1(LeaveDirectory, void(const SyncItem &entry));
  MOCK_METHOD1(Commit, bool(manifest::Manifest *manifest));
  MOCK_CONST_METHOD0(IsExternalData, bool());
  MOCK_CONST_METHOD0(GetCompressionAlgorithm, zlib::Algorithms());
};

}  // namespace publish
