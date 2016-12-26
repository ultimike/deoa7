<?php
/**
 * @file
 * Implementation of DrupalNodeRevisionMigration for Drupal 6 sources.
 */

/**
 * Handling specific to a Drupal 6 source for nodes.
 */
class DrupalNodeRevision6Migration extends DrupalNodeRevisionMigration {
  /**
   * Translation from field names assigned when executing the query to our
   * subfield notation.
   *
   * @var array
   *  key: DB-compatible name (e.g., field_buy_link_title).
   *  value: Subfield notation (e.g., field_buy_link:title).
   */
  protected $fixFieldNames = array();

  /**
   * Keep track of any file field data columns we'll need to unpack in
   * prepareRow().
   *
   * @var array
   */
  protected $fileDataFields = array();

  /**
   * @param array $arguments
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $query = $this->query();

    $this->sourceOptions['fix_field_names'] = $this->fixFieldNames;

    $this->source = new MigrateDrupal6SourceSQL($query, $this->sourceFields, NULL,
      $this->sourceOptions);

    if (field_info_instance('node', 'body', $this->destinationType)) {
      $this->addFieldMapping('body:summary', 'teaser');
      $this->addFieldMapping('body:format', 'format')
           ->callbacks(array($this, 'mapFormat'));
    }
    else {
      $this->addUnmigratedSources(array('teaser', 'format'));
    }
  }

  /**
   * Query for basic node fields from Drupal 6.
   *
   * @return QueryConditionInterface
   */
  protected function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('node_revisions', 'nr')
             ->fields('nr', array('nid', 'vid', 'uid', 'title', 'body',
                 'teaser', 'log', 'timestamp', 'format'))
             ->orderBy('nr.timestamp');
    $query->innerJoin('node', 'n', 'nr.nid=n.nid');
    $query->fields('n', array('status', 'promote', 'sticky', 'comment'));
    $query->condition('n.type', $this->sourceType);

    // Pick up simple CCK fields
    $cck_table = 'content_type_' . $this->sourceType;
    if (Database::getConnection('default', $this->sourceConnection)
          ->schema()->tableExists($cck_table)) {
      $query->leftJoin($cck_table, 'f', 'nr.vid=f.vid');
      // The main column for the field should be rendered with
      // the field name, not the column name (e.g., field_foo rather
      // than field_foo_value).
      $field_info = $this->version->getSourceFieldInfo();
      foreach ($field_info as $field_name => $info) {
        if (isset($info['columns']) && !$info['multiple'] && $info['db_storage']) {
          $i = 0;
          $data = FALSE;
          foreach ($info['columns'] as $display_name => $column_name) {
            if ($i++ == 0) {
              $query->addField('f', $column_name, $field_name);
            }
            else {
              // The database API won't allow colons in column aliases, so we
              // will accept the default alias, and fix up the field names later.
              // Remember how to translate the field names.
              $clean_name = str_replace(':', '_', $display_name);
              $this->fixFieldNames[$clean_name] = $display_name;
              if ($info['type'] == 'filefield' &&
                  (strpos($display_name, ':list') || strpos($display_name, ':description') || strpos($display_name, ':alt') || strpos($display_name, ':title'))) {
                if (!$data) {
                  $this->fileDataFields[] = $field_name . '_data';
                  $query->addField('f', $field_name . '_data');
                  $data = TRUE;
                }
              }
              else {
                $query->addField('f', $column_name);
              }
            }
          }
        }
      }
    }
    return $query;
  }

  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    foreach ($this->fileDataFields as $data_field) {
      if (isset($row->$data_field)) {
        $data = unserialize($row->$data_field);
        $base_field = substr($data_field, 0, strpos($data_field, '_data'));
        foreach ($data as $key => $value) {
          $field_name = $base_field . '_' . $key;
          $row->$field_name = $value;
        }
        unset($row->$data_field);
      }
    }
    // Convert the default field names to the nice-looking ones.
    foreach ($this->fixFieldNames as $clean => $display) {
      if (isset($row->$clean)) {
        $row->$display = $row->$clean;
        unset($row->$clean);
      }
    }

    // Don't populate summary if the teaser matches the generated summary.
    if (empty($row->teaser) || $row->teaser == text_summary($row->body)) {
      $row->teaser = '';
    }
  }
}
