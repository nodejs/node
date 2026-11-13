V8_AUTOROLL_ACCOUNT = "v8-ci-autoroll-builder@chops-service-accounts.iam.gserviceaccount.com"
V8_TRY_ACCOUNT = "v8-try-builder@chops-service-accounts.iam.gserviceaccount.com"
V8_CI_ACCOUNT = "v8-ci-builder@chops-service-accounts.iam.gserviceaccount.com"
V8_PGO_ACCOUNT = "v8-ci-pgo-builder@chops-service-accounts.iam.gserviceaccount.com"
V8_TEST262_EXPORT_ACCOUNT = "v8-ci-test262-import-export@chops-service-accounts.iam.gserviceaccount.com"
V8_TEST262_IMPORT_ACCOUNT = "v8-ci-autoroll-builder@chops-service-accounts.iam.gserviceaccount.com"

V8_HP_SERVICE_ACCOUNTS = [
    V8_AUTOROLL_ACCOUNT,
    V8_PGO_ACCOUNT,
    V8_TEST262_EXPORT_ACCOUNT,
    V8_TEST262_IMPORT_ACCOUNT,
]
V8_SERVICE_ACCOUNTS = [
    V8_TRY_ACCOUNT,
    V8_CI_ACCOUNT,
    V8_PGO_ACCOUNT,
]
