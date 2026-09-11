# Public repository

- Use synthetic people, calendar IDs, hosts and credentials in every tracked file,
  including configuration examples, tests, fixtures, reports and documentation.
- Never copy personal installation data from the conversation into tracked files.
- Keep actual board mappings in ignored `include/BoardConfig.h`; maintain only
  `include/BoardConfig.example.h` in Git.
- Keep credentials in ignored `src/secrets.h` and OTA settings in ignored
  `upload_params.ini`. Do not log their contents.
- Do not include absolute paths containing a user's account name in documentation.
- Before committing, check the staged diff for installation data. Do not stage
  ignored local configuration files, even when asked to commit implementation work.
