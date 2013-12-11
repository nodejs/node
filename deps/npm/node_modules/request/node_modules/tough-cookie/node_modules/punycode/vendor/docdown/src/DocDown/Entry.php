<?php

require(dirname(__FILE__) . "/Alias.php");

/**
 * A class to simplify parsing a single JSDoc entry.
 */
class Entry {

  /**
   * The documentation entry.
   *
   * @memberOf Entry
   * @type String
   */
  public $entry = '';

  /**
   * The language highlighter used for code examples.
   *
   * @memberOf Entry
   * @type String
   */
  public $lang = '';

  /**
   * The source code.
   *
   * @memberOf Entry
   * @type String
   */
  public $source = '';

  /*--------------------------------------------------------------------------*/

  /**
   * The Entry constructor.
   *
   * @constructor
   * @param {String} $entry The documentation entry to analyse.
   * @param {String} $source The source code.
   * @param {String} [$lang ='js'] The language highlighter used for code examples.
   */
  public function __construct( $entry, $source, $lang = 'js' ) {
    $this->entry = $entry;
    $this->lang = $lang;
    $this->source = str_replace(PHP_EOL, "\n", $source);
  }

  /*--------------------------------------------------------------------------*/

  /**
   * Extracts the documentation entries from source code.
   *
   * @static
   * @memberOf Entry
   * @param {String} $source The source code.
   * @returns {Array} The array of entries.
   */
  public static function getEntries( $source ) {
    preg_match_all('#/\*\*(?![-!])[\s\S]*?\*/\s*.+#', $source, $result);
    return array_pop($result);
  }

  /*--------------------------------------------------------------------------*/

  /**
   * Checks if the entry is a function reference.
   *
   * @private
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if the entry is a function reference, else `false`.
   */
  private function isFunction() {
    if (!isset($this->_isFunction)) {
      $this->_isFunction = !!(
        $this->isCtor() ||
        count($this->getParams()) ||
        count($this->getReturns()) ||
        preg_match('/\*[\t ]*@function\b/', $this->entry)
      );
    }
    return $this->_isFunction;
  }

  /*--------------------------------------------------------------------------*/

  /**
   * Extracts the entry's `alias` objects.
   *
   * @memberOf Entry
   * @param {Number} $index The index of the array value to return.
   * @returns {Array|String} The entry's `alias` objects.
   */
  public function getAliases( $index = null ) {
    if (!isset($this->_aliases)) {
      preg_match('#\*[\t ]*@alias\s+(.+)#', $this->entry, $result);

      if (count($result)) {
        $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]));
        $result = preg_split('/,\s*/', $result);
        natsort($result);

        foreach ($result as $resultIndex => $value) {
          $result[$resultIndex] = new Alias($value, $this);
        }
      }
      $this->_aliases = $result;
    }
    return $index !== null
      ? @$this->_aliases[$index]
      : $this->_aliases;
  }

  /**
   * Extracts the function call from the entry.
   *
   * @memberOf Entry
   * @returns {String} The function call.
   */
  public function getCall() {
    if (isset($this->_call)) {
      return $this->_call;
    }

    preg_match('#\*/\s*(?:function ([^(]*)|(.*?)(?=[:=,]|return\b))#', $this->entry, $result);
    if ($result = array_pop($result)) {
      $result = array_pop(explode('var ', trim(trim(array_pop(explode('.', $result))), "'")));
    }
    // resolve name
    // avoid $this->getName() because it calls $this->getCall()
    preg_match('#\*[\t ]*@name\s+(.+)#', $this->entry, $name);
    if (count($name)) {
      $name = trim($name[1]);
    } else {
      $name = $result;
    }
    // compile function call syntax
    if ($this->isFunction()) {
      // compose parts
      $result = array($result);
      $params = $this->getParams();
      foreach ($params as $param) {
        $result[] = $param[1];
      }
      // format
      $result = $name .'('. implode(array_slice($result, 1), ', ') .')';
      $result = str_replace(', [', ' [, ', str_replace('], [', ', ', $result));
    }

    $this->_call = $result ? $result : $name;
    return $this->_call;
  }

  /**
   * Extracts the entry's `category` data.
   *
   * @memberOf Entry
   * @returns {String} The entry's `category` data.
   */
  public function getCategory() {
    if (isset($this->_category)) {
      return $this->_category;
    }

    preg_match('#\*[\t ]*@category\s+(.+)#', $this->entry, $result);
    if (count($result)) {
      $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]));
    } else {
      $result = $this->getType() == 'Function' ? 'Methods' : 'Properties';
    }
    $this->_category = $result;
    return $result;
  }

  /**
   * Extracts the entry's description.
   *
   * @memberOf Entry
   * @returns {String} The entry's description.
   */
  public function getDesc() {
    if (isset($this->_desc)) {
      return $this->_desc;
    }

    preg_match('#/\*\*(?:\s*\*)?([\s\S]*?)(?=\*\s\@[a-z]|\*/)#', $this->entry, $result);
    if (count($result)) {
      $type = $this->getType();
      $result = preg_replace('/:\n[\t ]*\*[\t ]*/', ":<br>\n", $result[1]);
      $result = preg_replace('/(?:^|\n)[\t ]*\*\n[\t ]*\*[\t ]*/', "\n\n", $result);
      $result = preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result);
      $result = trim($result);
      $result = ($type == 'Function' ? '' : '(' . str_replace('|', ', ', trim($type, '{}')) . '): ') . $result;
    }
    $this->_desc = $result;
    return $result;
  }

  /**
   * Extracts the entry's `example` data.
   *
   * @memberOf Entry
   * @returns {String} The entry's `example` data.
   */
  public function getExample() {
    if (isset($this->_example)) {
      return $this->_example;
    }

    preg_match('#\*[\t ]*@example\s+([\s\S]*?)(?=\*\s\@[a-z]|\*/)#', $this->entry, $result);
    if (count($result)) {
      $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', "\n", $result[1]));
      $result = '```' . $this->lang . "\n" . $result . "\n```";
    }
    $this->_example = $result;
    return $result;
  }

  /**
   * Checks if the entry is an alias.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `false`.
   */
  public function isAlias() {
    return false;
  }

  /**
   * Checks if the entry is a constructor.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if a constructor, else `false`.
   */
  public function isCtor() {
    if (!isset($this->_isCtor)) {
      $this->_isCtor = !!preg_match('/\*[\t ]*@constructor\b/', $this->entry);
    }
    return $this->_isCtor;
  }

  /**
   * Checks if the entry is a license.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if a license, else `false`.
   */
  public function isLicense() {
    if (!isset($this->_isLicense)) {
      $this->_isLicense = !!preg_match('/\*[\t ]*@license\b/', $this->entry);
    }
    return $this->_isLicense;
  }

  /**
   * Checks if the entry *is* assigned to a prototype.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if assigned to a prototype, else `false`.
   */
  public function isPlugin() {
    if (!isset($this->_isPlugin)) {
      $this->_isPlugin = !$this->isCtor() && !$this->isPrivate() && !$this->isStatic();
    }
    return $this->_isPlugin;
  }

  /**
   * Checks if the entry is private.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if private, else `false`.
   */
  public function isPrivate() {
    if (!isset($this->_isPrivate)) {
      $this->_isPrivate = $this->isLicense() || !!preg_match('/\*[\t ]*@private\b/', $this->entry) || !preg_match('/\*[\t ]*@[a-z]+\b/', $this->entry);
    }
    return $this->_isPrivate;
  }

  /**
   * Checks if the entry is *not* assigned to a prototype.
   *
   * @memberOf Entry
   * @returns {Boolean} Returns `true` if not assigned to a prototype, else `false`.
   */
  public function isStatic() {
    if (isset($this->_isStatic)) {
      return $this->_isStatic;
    }

    $public = !$this->isPrivate();
    $result = $public && !!preg_match('/\*[\t ]*@static\b/', $this->entry);

    // set in cases where it isn't explicitly stated
    if ($public && !$result) {
      if ($parent = array_pop(preg_split('/[#.]/', $this->getMembers(0)))) {
        foreach (Entry::getEntries($this->source) as $entry) {
          $entry = new Entry($entry, $this->source);
          if ($entry->getName() == $parent) {
            $result = !$entry->isCtor();
            break;
          }
        }
      } else {
        $result = true;
      }
    }
    $this->_isStatic = $result;
    return $result;
  }

  /**
   * Resolves the entry's line number.
   *
   * @memberOf Entry
   * @returns {Number} The entry's line number.
   */
  public function getLineNumber() {
    if (!isset($this->_lineNumber)) {
      preg_match_all('/\n/', substr($this->source, 0, strrpos($this->source, $this->entry) + strlen($this->entry)), $lines);
      $this->_lineNumber = count(array_pop($lines)) + 1;
    }
    return $this->_lineNumber;
  }

  /**
   * Extracts the entry's `member` data.
   *
   * @memberOf Entry
   * @param {Number} $index The index of the array value to return.
   * @returns {Array|String} The entry's `member` data.
   */
  public function getMembers( $index = null ) {
    if (!isset($this->_members)) {
      preg_match('#\*[\t ]*@member(?:Of)?\s+(.+)#', $this->entry, $result);
      if (count($result)) {
        $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]));
        $result = preg_split('/,\s*/', $result);
        natsort($result);
      }
      $this->_members = $result;
    }
    return $index !== null
      ? @$this->_members[$index]
      : $this->_members;
  }

  /**
   * Extracts the entry's `name` data.
   *
   * @memberOf Entry
   * @returns {String} The entry's `name` data.
   */
  public function getName() {
    if (isset($this->_name)) {
      return $this->_name;
    }

    preg_match('#\*[\t ]*@name\s+(.+)#', $this->entry, $result);
    if (count($result)) {
      $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]));
    } else {
      $result = array_shift(explode('(', $this->getCall()));
    }
    $this->_name = $result;
    return $result;
  }

  /**
   * Extracts the entry's `param` data.
   *
   * @memberOf Entry
   * @param {Number} $index The index of the array value to return.
   * @returns {Array} The entry's `param` data.
   */
  public function getParams( $index = null ) {
    if (!isset($this->_params)) {
      preg_match_all('#\*[\t ]*@param\s+\{([^}]+)\}\s+(\[.+\]|[$\w|]+(?:\[.+\])?)\s+([\s\S]*?)(?=\*\s\@[a-z]|\*/)#i', $this->entry, $result);
      if (count($result = array_filter(array_slice($result, 1)))) {
        // repurpose array
        foreach ($result as $param) {
          foreach ($param as $key => $value) {
            if (!is_array($result[0][$key])) {
              $result[0][$key] = array();
            }
            $result[0][$key][] = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]*/', ' ', $value));
          }
        }
        $result = $result[0];
      }
      $this->_params = $result;
    }
    return $index !== null
      ? @$this->_params[$index]
      : $this->_params;
  }

  /**
   * Extracts the entry's `returns` data.
   *
   * @memberOf Entry
   * @returns {String} The entry's `returns` data.
   */
  public function getReturns() {
    if (isset($this->_returns)) {
      return $this->_returns;
    }

    preg_match('#\*[\t ]*@returns\s+\{([^}]+)\}\s+([\s\S]*?)(?=\*\s\@[a-z]|\*/)#', $this->entry, $result);
    if (count($result)) {
      $result = array_map('trim', array_slice($result, 1));
      $result[0] = str_replace('|', ', ', $result[0]);
      $result[1] = preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]);
    }
    $this->_returns = $result;
    return $result;
  }

  /**
   * Extracts the entry's `type` data.
   *
   * @memberOf Entry
   * @returns {String} The entry's `type` data.
   */
  public function getType() {
    if (isset($this->_type)) {
      return $this->_type;
    }

    preg_match('#\*[\t ]*@type\s+(.+)#', $this->entry, $result);
    if (count($result)) {
      $result = trim(preg_replace('/(?:^|\n)[\t ]*\*[\t ]?/', ' ', $result[1]));
    } else {
      $result = $this->isFunction() ? 'Function' : 'Unknown';
    }
    $this->_type = $result;
    return $result;
  }
}
?>