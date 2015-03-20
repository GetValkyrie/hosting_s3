<?php

/**
 * @file
 * Drush integration for the S3 service.
 */

/**
 * Implements hook_hosting_site_context_options().
 */
function hosting_s3_hosting_site_context_options(&$task) {
  $fields = array('s3_access_key_id', 's3_secret_access_key', 's3_bucket_name');
  foreach ($fields as $field) {
    if (isset($task->ref->$field) && !empty($task->ref->$field)) {
      $task->context_options[$field] = $task->ref->$field;
    }
    else {
      $task->context_options[$field] = 'null';
    }
  }
}

/**
 * Implements hook_drush_context_import().
 */
function hosting_s3_drush_context_import($context, &$node) {
  if ($context->type == 'site') {
    $fields = array('s3_access_key_id', 's3_secret_access_key', 's3_bucket_name');
    foreach ($fields as $field) {
      if (isset($context->$field) && !empty($context->$field)) {
        $node->$field = $context->$field;
      }
    }
  }
}
