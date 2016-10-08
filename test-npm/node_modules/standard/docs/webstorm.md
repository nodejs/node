# [WebStorm/PhpStorm][webstorm-1] configuration for Standard Style

1. Turn off your IDE
2. [Figure out where your configuration lives][webstorm-2] (_IDE Settings_ section)
3. Navigate to `your-config-dir/codestyles`. If this directory doesn't exist, create it in the WebStorm
config settings directory.
4. Create a `Standard.xml` file:

   ```xml
  <code_scheme name="Standard">
    <JSCodeStyleSettings>
      <option name="USE_SEMICOLON_AFTER_STATEMENT" value="false" />
      <option name="SPACES_WITHIN_OBJECT_LITERAL_BRACES" value="true" />
    </JSCodeStyleSettings>
    <XML>
      <option name="XML_LEGACY_SETTINGS_IMPORTED" value="true" />
    </XML>
    <codeStyleSettings language="JavaScript">
      <option name="KEEP_BLANK_LINES_IN_CODE" value="1" />
      <option name="SPACE_WITHIN_BRACKETS" value="true" />
      <option name="SPACE_BEFORE_METHOD_PARENTHESES" value="true" />
      <option name="KEEP_SIMPLE_BLOCKS_IN_ONE_LINE" value="true" />
      <option name="KEEP_SIMPLE_METHODS_IN_ONE_LINE" value="true" />
      <indentOptions>
        <option name="INDENT_SIZE" value="2" />
        <option name="CONTINUATION_INDENT_SIZE" value="2" />
        <option name="TAB_SIZE" value="2" />
      </indentOptions>
    </codeStyleSettings>
  </code_scheme>
  ```

5. If you don't want to clutter your project with extra dependencies and config files, then go global:

   - `npm install -g eslint eslint-config-standard eslint-plugin-standard eslint-plugin-promise` (`sudo` might be required)
   - `echo '{"extends": ["standard"]}' > ~/.eslintrc` (be warned: it overrides an existing file)

6. If you're OK with local dependencies and config, do (5) without `-g` and `~/`
7. Fire up the IDE and open a _Settings_/_Preferences_ screen (choose between project and default settings accordingly to your preference)
8. Under `Editor > Code Style > JavaScript` change `Scheme` to `Standard`
9. Under `Editor > Inspections > JavaScript > Code style issues` untick `Unterminated statement`
10. Under `Languages & Frameworks > JavaScript > Code Quality Tools > ESLint` just select `Enable`, only in rare cases you'll have to configure `ESLint package` path

[webstorm-1]: https://www.jetbrains.com/webstorm/
[webstorm-2]: https://www.jetbrains.com/webstorm/help/project-and-ide-settings.html
