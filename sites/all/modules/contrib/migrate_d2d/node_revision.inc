<?php
/**
 * @file
 * Base class for migrating node revisions into Drupal.
 */

/**
 * Base class for all node revision migrations - handles commonalities across all
 * supported source Drupal versions.
 *
 * In addition to the arguments supported by DrupalMigration, the following
 * must be passed in the $arguments array:
 *
 * source_type - Drupal 6 content type machine name.
 * destination_type - Drupal 7 content type machine name (bundle).
 * node_migration - Machine name of a node migration, used to map the nids.
 *
 * The following optional arguments may be passed:
 *
 * user_migration - Machine name of a user migration, used to establish
 *   dependencies and a sourceMigration for the uid mapping.
 * default_uid - Drupal 7 (destination) uid of the user account to use as
 *   the default.
 * default_language - Default language for the node and node body. Defaults
 *   to LANGUAGE_NONE.
 */
abstract class DrupalNodeRevisionMigration extends DrupalMigration {
  /**
   * The source and destination content types (bundles) we're dealing with.
   */
  protected $destinationType;

  /**
   * Default language to apply to the node and it's body field.
   *
   * @var string
   */
  protected $defaultLanguage = LANGUAGE_NONE;

  /**
   * @param array $arguments
   */
  public function __construct(array $arguments) {
    $this->destinationType = $arguments['destination_type'];
    $this->sourceType = $arguments['source_type'];
    $this->dependencies[] = $arguments['node_migration'];
    if (!empty($arguments['user_migration'])) {
      $user_migration = $arguments['user_migration'];
      $this->dependencies[] = $user_migration;
    }
    if (!empty($arguments['default_language'])) {
      $this->defaultLanguage = $arguments['default_language'];
    }
    parent::__construct($arguments);

    // Document known core fields
    $this->sourceFields += array(
      'nid' => t('Node ID'),
      'vid' => t('Revision ID'),
      'nid' => t('Authored by (uid)'),
      'uid' => t('Authored by (uid)'),
      'title' => t('Title'),  // @todo Depends on has_title, and label may be customized
      'body' => t('Body'),
      'format' => t('Format'),
      'teaser' => t('Teaser'),
      'timestamp' => t('Created timestamp'),
      'log' => t('Revision Log message'),
      'status' => t('Published'),
      'promote' => t('Promoted to front page'),
      'sticky' => t('Sticky at top of lists'),
    );

    $this->sourceFields += $this->version->getSourceFields('node', $this->sourceType);
    $this->destination = new MigrateDestinationNodeRevision($this->destinationType);

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'vid' => array('type' => 'int',
                       'unsigned' => TRUE,
                       'not null' => TRUE,
                       'description' => 'Source revision ID',
                       'alias' => 'nr',
                      ),
      ),
      MigrateDestinationNodeRevision::getKeySchema()
    );

    if (!$this->newOnly) {
      $this->highwaterField = array(
        'name' => 'timestamp',
        'alias' => 'nr',
        'type' => 'int',
      );
    }

    // Setup common mappings
    $this->addSimpleMappings(array('title', 'log', 'status',
      'promote', 'sticky', 'comment'));
    $this->addFieldMapping('changed', 'timestamp');
    $this->addFieldMapping('nid', 'nid')
         ->sourceMigration($arguments['node_migration']);

    if (field_info_instance('node', 'body', $this->destinationType)) {
      $this->addFieldMapping('body', 'body');
      $this->addFieldMapping('body:language')
           ->defaultValue($this->defaultLanguage);
    }
    else {
      $this->addUnmigratedSources(array('body'));
    }

    if (isset($arguments['default_uid'])) {
      $default_uid = $arguments['default_uid'];
    }
    else {
      $default_uid = 1;
    }
    if (isset($user_migration)) {
      $this->addFieldMapping('uid', 'uid')
           ->sourceMigration($user_migration)
           ->defaultValue($default_uid);
      $this->addFieldMapping('revision_uid', 'uid')
           ->sourceMigration($user_migration)
           ->defaultValue($default_uid);
    }
    else {
      $this->addFieldMapping('uid')
           ->defaultValue($default_uid);
      $this->addFieldMapping('revision_uid')
           ->defaultValue($default_uid);
    }
  }

  /**
   * Implementation of Migration::prepareRow().
   */
  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    $this->version->getSourceValues($row, $row->nid);
  }
}
