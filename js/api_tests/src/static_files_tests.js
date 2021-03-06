'use strict';

const path = require('path');
const { promisify } = require('util');
const readFile = promisify(require('fs').readFile);
const q = require('qunit');

const { client, usePlayer, config } = require('./test_context');

q.module('static files', usePlayer());

function getFile(url, config)
{
    return client.handler.axios.get(url, config);
}

function getFileData(url)
{
   return readFile(path.join(config.webRootDir, url), 'utf8');
}

q.test('get index', async assert =>
{
    const result = await getFile('');
    assert.equal(result.data, 'index.html\n');
});

q.test('get file', async assert =>
{
    const result = await getFile('file.html');
    assert.equal(result.data, 'file.html\n');
});

q.test('get subdir index', async assert =>
{
    const result = await getFile('subdir/');
    assert.equal(result.data, 'subdir/index.html\n');
});

q.test('get subdir file', async assert =>
{
    const result = await getFile('subdir/file.html');
    assert.equal(result.data, 'subdir/file.html\n');
});

q.test('provide content type', async assert =>
{
    const contentTypes = {
        'file.html': 'text/html',
        'file.htm': 'text/html',
        'file.css': 'text/css',
        'file.svg': 'image/svg+xml',
        'file.js': 'application/javascript',
        'file.png': 'image/png',
        'file.jpeg': 'image/jpeg',
        'file.jpg': 'image/jpeg',
        'file.txt': 'text/plain',
    };

    for (let file of Object.keys(contentTypes))
    {
        const result = await getFile(file);
        assert.equal(result.headers['content-type'], contentTypes[file]);
    }
});

q.test('etag support', async assert =>
{
    const initialResult = await getFile('file.html');

    const etag = initialResult.headers['etag'];
    assert.ok(etag);

    const cacheControl = initialResult.headers['cache-control'];
    assert.equal(cacheControl, 'max-age=3, must-revalidate');

    const cachedResult = await getFile('file.html', {
        headers: { 'If-None-Match': etag },
        validateStatus: () => true,
    });

    assert.equal(cachedResult.status, 304);
    assert.equal(cachedResult.headers['etag'], etag);
    assert.equal(cachedResult.headers['cache-control'], cacheControl);
});

q.test('enable compression', async assert =>
{
    const expectedData = await getFileData('large.txt');
    const result = await getFile('large.txt', {
        headers: { 'Accept-Encoding': 'whatever, gzip' },
    });

    assert.equal(result.data, expectedData);

    const contentLength = parseInt(result.headers['content-length']);
    assert.ok(contentLength < expectedData.length);
});

q.test('disable compression', async assert =>
{
    const expectedData = await getFileData('large.txt');
    const result = await getFile('large.txt', {
        headers: { 'Accept-Encoding': 'whatever' },
    });

    assert.equal(result.data, expectedData);

    const contentLength = parseInt(result.headers['content-length']);
    assert.equal(contentLength, expectedData.length);
});

q.test('get via parent directory', async assert =>
{
    const expectedData = await getFileData('file.txt');

    const result1 = await getFile('/test/../file.txt');
    assert.equal(result1.data, expectedData);

    const result2 = await getFile('/test/%2E%2E/file.txt');
    assert.equal(result2.data, expectedData);
});

q.test('escape root dir', async assert =>
{
    const ignoreStatus = {
        validateStatus: () => true,
    };

    const result1 = await getFile('/../package.json', ignoreStatus);
    assert.equal(result1.status, 404);

    const result2 = await getFile('/%2E%2E/package.json', ignoreStatus);
    assert.equal(result2.status, 404);
});
