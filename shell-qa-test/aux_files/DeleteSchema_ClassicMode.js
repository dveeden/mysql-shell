session.runSql("DROP DATABASE IF EXISTS schema_test;");

session.runSql("CREATE SCHEMA schema_test;");
session.getSchemas();

session.dropSchema('schema_test');

session.getSchemas();
