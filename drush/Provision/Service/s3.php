<?php

/**
 * The s3 service class.
 */
class Provision_Service_s3 extends Provision_Service {
  public $service = 's3';

  /**
   * Add the needed properties to the site context.
   */
  static function subscribe_site($context) {
    $context->setProperty('access_key_id');
    $context->setProperty('secret_access_key');
  }

  /**
   * Wrapper around hook_provision_drupal_create_directories_alter().
   */
  function create_directories_alter(&$dirs, $url) {
    // Create sites/$url and ensure permissions are set (remove from $dirs).
    // This helper can be called optionally to ensure that the site directory
    // is created appropriately before applying other modifications.
    $path = 'sites/' . $url;
    if (!is_dir($path)) {
      provision_file()->mkdir($path)
        ->succeed('Created <code>@path</code>')
        ->fail('Could not create <code>@path</code>', 'DRUSH_PERM_ERROR');
    }

    provision_file()->chmod($path, $dirs[$path], false)
      ->succeed('Changed permissions of <code>@path</code> to @perm')
      ->fail('Could not change permissions <code>@path</code> to @perm');
    unset($dirs[$path]);
  }

  /**
   * Wrapper around hook_provision_provision_drupal_chgrp_directories_alter().
   */
  function chgrp_directories_alter(&$chgrp, $url) {
    // Do nothing here. The function is needed because it's always called,
    // but the actual implementation will be in the storage engine.
  }
}
